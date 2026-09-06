import Foundation
import CoreBluetooth
import CryptoKit

// Upload only the seven generated Faba catalog assets. Existing differing files
// are backed up locally before replacement, and each upload is read back.
setbuf(stdout, nil)
let selectedPixlID: UUID = {
 guard let targetText = ProcessInfo.processInfo.environment["PIXL_UUID"],
       let identifier = UUID(uuidString: targetText) else {
  print("Set PIXL_UUID to the exact application UUID from pixl-scan. No device selected.")
  exit(2)
 }
 return identifier
}()

let serviceID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
let writeID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
let notifyID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
struct Asset: Decodable { let file: String; let device_path: String; let bytes: Int; let sha256: String }
struct Manifest: Decodable { let entries: Int; let files: [Asset] }
class CatalogUploader: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
 var central: CBCentralManager!
 var target: CBPeripheral?
 var tx: CBCharacteristic?
 var rx: CBCharacteristic?
 var done = false, success = false
 var received = Data(), chunk = 0, command: UInt8 = 1
 var fileID: UInt8 = 0
 var index = 0, offset = 0
 var stage = "version"
 let directory: URL, backup: URL, manifest: Manifest
 let contents: [Data]
 init(directory: URL, backup: URL) throws {
  self.directory = directory; self.backup = backup
  manifest = try JSONDecoder().decode(Manifest.self, from: Data(contentsOf: directory.appendingPathComponent("manifest.json")))
  guard manifest.entries == 257, manifest.files.count == 7 else { throw NSError(domain: "Invalid catalog", code: 1) }
  contents = try manifest.files.enumerated().map { i, file in
   guard file.file == "cat\(i).bin", file.device_path == "E:/faba/cat\(i).bin" else { throw NSError(domain: "Invalid destination", code: 1) }
   let data = try Data(contentsOf: directory.appendingPathComponent(file.file))
   let sha = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
   guard data.count == file.bytes, sha == file.sha256 else { throw NSError(domain: "Catalog checksum mismatch", code: 1) }
   return data
  }
  guard !FileManager.default.fileExists(atPath: backup.path) else { throw NSError(domain: "Use a fresh backup directory", code: 1) }
  try FileManager.default.createDirectory(at: backup, withIntermediateDirectories: true)
  super.init()
 }
 func start() {
  print("Bluetooth authorization: \(CBManager.authorization.rawValue)")
  central = CBCentralManager(delegate: self, queue: nil)
 }
 func finish(_ message: String) {
  print(message)
  central.stopScan()
  if let p = target { central.cancelPeripheralConnection(p) }
  done = true
 }
 func centralManagerDidUpdateState(_ central: CBCentralManager) {
  print("Bluetooth state: \(central.state.rawValue)")
  if central.state == .poweredOn {
   print("Scanning for Pixl UART service...")
   central.scanForPeripherals(withServices: [serviceID], options: nil)
  } else if central.state == .unauthorized || central.state == .unsupported || central.state == .poweredOff {
   finish("Bluetooth unavailable: \(central.state.rawValue)")
  }
 }
 func centralManager(_ central: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
  let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? p.name ?? "Unnamed UART device"
  print("Found: \(name), id=\(p.identifier), RSSI=\(RSSI)")
  guard p.identifier == selectedPixlID else { return }
  guard target == nil else { return }
  target = p; p.delegate = self
  central.stopScan(); central.connect(p, options: nil)
 }
 func centralManager(_ central: CBCentralManager, didConnect p: CBPeripheral) {
  print("Connected: \(p.name ?? "Pixl")")
  p.discoverServices([serviceID])
 }
 func centralManager(_ central: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) { finish("Connection failed: \(String(describing: error))") }
 func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
  if let e = error { finish("Services error: \(e)"); return }
  for s in p.services ?? [] { p.discoverCharacteristics([writeID, notifyID], for: s) }
 }
 func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
  if let e = error { finish("Characteristics error: \(e)"); return }
  for c in service.characteristics ?? [] {
   if c.uuid == writeID { tx = c }
   if c.uuid == notifyID { rx = c }
  }
  if let r = rx, tx != nil { p.setNotifyValue(true, for: r) }
 }
 func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor c: CBCharacteristic, error: Error?) {
  if let e = error { finish("Notification error: \(e)"); return }
  guard c.isNotifying, let t = tx else { return }
  print("Requesting firmware version")
  p.writeValue(Data([0x01,0,0,0]), for: t, type: t.properties.contains(.write) ? .withResponse : .withoutResponse)
 }
 func encoded(_ path: String) -> [UInt8] {
  let b = Array(path.utf8); return [UInt8(b.count), 0] + b
 }
 func send(_ cmd: UInt8, payload: [UInt8] = []) {
  guard let p = target, let t = tx else { return }
  received = Data(); chunk = 0; command = cmd
  DispatchQueue.main.asyncAfter(deadline: .now() + 0.03) {
   guard !self.done else { return }
   p.writeValue(Data([cmd,0,0,0] + payload), for: t, type: t.properties.contains(.write) ? .withResponse : .withoutResponse)
  }
 }
 func open(_ flags: UInt8) { send(0x12, payload: encoded(manifest.files[index].device_path) + [flags,0,0,0]) }
 func nextFile() {
  if index == contents.count { success = true; finish("CATALOG VERIFIED: all 257 entries in seven files match after read-back"); return }
  print("Checking " + manifest.files[index].device_path)
  stage = "checkOpen"; open(8)
 }
 func writeNextChunk() {
  let data = contents[index]
  if offset == data.count { stage = "writeClose"; send(0x13,payload:[fileID]); return }
  let end = min(offset + 180, data.count)
  let block = Array(data[offset..<end]); offset = end
  send(0x15,payload:[fileID] + block)
 }
 func beginWrite() { stage = "writeOpen"; offset = 0; open(22) }
 func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
  if let e = error { finish("Read failed: \(e)"); return }
  guard let data = c.value else { return }
  let b = [UInt8](data)
  guard b.count >= 4, b[0] == command else { finish("Unexpected response"); return }
  let sequence = Int(b[2]) | ((Int(b[3]) & 0x7f) << 8)
  guard sequence == chunk else { finish("Unexpected chunk sequence"); return }; chunk += 1
  if b[1] != 0 {
   if stage == "folderCheck", b[1] == 166 { stage = "folderCreate"; send(0x17,payload:encoded("E:/faba")); return }
   if stage == "checkOpen", b[1] == 166 { beginWrite(); return }
   // SPIFFS can report an absent file at READ rather than OPEN. Only the
   // exact NOOBJ status with no partial data permits creating a new asset.
   if stage == "checkRead", b[1] == 166, received.isEmpty, sequence == 0 {
    stage = "checkClose"; send(0x13,payload:[fileID]); return
   }
   finish("Device rejected command \(command), status \(b[1]); reconnect and retry"); return
  }
  received.append(contentsOf: b.dropFirst(4))
  guard received.count <= 30000 else { finish("Unexpected file size"); return }
  if b[3] & 0x80 != 0 { return }
  let body = received
  switch stage {
  case "version":
   guard let t=tx, p.maximumWriteValueLength(for:t.properties.contains(.write) ? .withResponse : .withoutResponse) >= 185 else { finish("BLE packet size too small"); return }
   do { try body.write(to:backup.appendingPathComponent("version-response.bin"),options:.withoutOverwriting) }
   catch { finish("Cannot save version backup: \(error)"); return }
   stage="folderCheck"; send(0x16,payload:encoded("E:/faba"))
  case "folderCheck", "folderCreate": nextFile()
  case "checkOpen":
   guard let id=body.first else { finish("Missing file handle");return };fileID=id
   stage="checkRead";send(0x14,payload:[fileID])
  case "checkRead":
   if body == contents[index] { stage="skipClose" }
   else {
    do { try body.write(to:backup.appendingPathComponent(manifest.files[index].file),options:.withoutOverwriting) }
    catch { finish("Cannot back up existing catalog: \(error)"); return }
    stage="checkClose"
   }
   send(0x13,payload:[fileID])
  case "skipClose": print("Already matches");index+=1;nextFile()
  case "checkClose": beginWrite()
  case "writeOpen":
   guard let id=body.first else { finish("Missing file handle");return };fileID=id
   stage="writing";writeNextChunk()
  case "writing": writeNextChunk()
  case "writeClose": stage="verifyOpen";open(8)
  case "verifyOpen":
   guard let id=body.first else { finish("Missing file handle");return };fileID=id
   stage="verifyRead";send(0x14,payload:[fileID])
  case "verifyRead":
   guard body==contents[index] else { finish("Read-back mismatch; reconnect and retry");return }
   stage="verifyClose";send(0x13,payload:[fileID])
  case "verifyClose": print("Verified " + manifest.files[index].file);index+=1;nextFile()
  default: finish("Unexpected protocol state")
  }
 }
 func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
  if !done { finish("Pixl disconnected; reconnect and retry") }
 }
}
guard CommandLine.arguments.count == 3 else { print("Usage: pixl-upload-catalog CATALOG_DIRECTORY FRESH_BACKUP_DIRECTORY");exit(2) }
do {
 let uploader = try CatalogUploader(directory:URL(fileURLWithPath:CommandLine.arguments[1]),backup:URL(fileURLWithPath:CommandLine.arguments[2]))
 uploader.start()
 let deadline=Date().addingTimeInterval(240)
 while !uploader.done && Date()<deadline { RunLoop.current.run(until:Date().addingTimeInterval(0.1)) }
 if !uploader.done { uploader.finish("Timed out; reconnect and retry") }
 exit(uploader.success ? 0 : 1)
} catch { print("Preparation failed: \(error)");exit(1) }
