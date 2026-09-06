import Foundation
import CoreBluetooth

setbuf(stdout, nil)
let selectedPixlID: UUID = {
 guard let targetText = ProcessInfo.processInfo.environment["PIXL_UUID"],
       let identifier = UUID(uuidString: targetText) else {
  print("Set PIXL_UUID to the exact application UUID from pixl-scan. No device selected.")
  exit(2)
 }
 return identifier
}()

guard CommandLine.arguments.count == 2 else { print("Usage: pixl-read-slots <new-backup-directory>"); exit(2) }

guard let slotText = ProcessInfo.processInfo.environment["PIXL_SLOT_FILES"] else {
 print("Set PIXL_SLOT_FILES to config.bin and every saved slot filename, comma-separated.")
 exit(2)
}
let selectedSlotFiles = slotText.split(separator: ",").map(String.init)
guard selectedSlotFiles.contains("config.bin"),
      Set(selectedSlotFiles).count == selectedSlotFiles.count,
      selectedSlotFiles.allSatisfy({ $0 == "config.bin" || $0.range(of: "^[0-9a-fA-F]{2}\\.bin$", options: .regularExpression) != nil }) else {
 print("Invalid slot inventory. Use config.bin and two-digit hexadecimal .bin filenames.")
 exit(2)
}

let outputDirectory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
guard !FileManager.default.fileExists(atPath: outputDirectory.path) else { print("Refusing to overwrite existing backup directory"); exit(2) }
try FileManager.default.createDirectory(at: outputDirectory, withIntermediateDirectories: true)

let serviceID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
let writeID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
let notifyID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
class Probe: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
 var central: CBCentralManager!
 var target: CBPeripheral?
 var tx: CBCharacteristic?
 var rx: CBCharacteristic?
 var done = false
 var success = false
 var stage = 0
 var received = Data()
 let files = selectedSlotFiles
 var fileIndex = 0
 var fileID: UInt8 = 0
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
 func send(_ command: UInt8, payload: [UInt8] = []) {
  guard let p = target, let t = tx else { return }
  received = Data()
  DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
   p.writeValue(Data([command,0,0,0] + payload), for: t, type: t.properties.contains(.write) ? .withResponse : .withoutResponse)
  }
 }
 func openNext() {
  if fileIndex >= files.count { success = true; finish("Saved slot backups successfully"); return }
  let path = "E:/chameleon/slots/" + files[fileIndex]
  let bytes = Array(path.utf8)
  print("Reading " + path)
  send(0x12, payload: [UInt8(bytes.count & 255), UInt8(bytes.count >> 8)] + bytes + [8,0,0,0])
 }
 func list(_ path: String) {
  let bytes = Array(path.utf8)
  print("Listing " + path)
  send(0x16, payload: [UInt8(bytes.count & 255), UInt8(bytes.count >> 8)] + bytes)
 }
 func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
  if let e = error { finish("Read error: \(e)"); return }
  guard let data = c.value else { return }
  let b = [UInt8](data)
  guard b.count >= 4 else { return }
  print("Response cmd=\(b[0]) status=\(b[1]) chunk=\(Int(b[2]) | (Int(b[3]) << 8)) bytes=\(b.count)")
  received.append(contentsOf: b.dropFirst(4))
  if b[3] & 0x80 != 0 { return }
  let body = [UInt8](received)
  if b[0] == 1 && b[1] == 0 && body.count >= 2 {
   let n = Int(body[0]) | (Int(body[1]) << 8)
   if body.count >= 2+n { print("Firmware: " + String(decoding: body[2..<2+n], as: UTF8.self)) }
   openNext()
  } else if b[0] == 0x12 {
   guard b[1] == 0, let id = body.first else { finish("Open failed"); return }
   fileID = id; send(0x14, payload: [fileID])
  } else if b[0] == 0x14 {
   guard b[1] == 0 else { finish("Read failed"); return }
   do {
    let url = outputDirectory.appendingPathComponent(files[fileIndex])
    try Data(body).write(to: url)
    print("Saved \(body.count) bytes to \(url.path)")
   } catch { finish("Save failed: \(error)"); return }
   send(0x13, payload: [fileID])
  } else if b[0] == 0x13 {
   guard b[1] == 0 else { finish("Close failed"); return }
   fileIndex += 1; openNext()
  } else if b[0] == 0x10 {
   print("Disk response: " + body.map { String(format:"%02x",$0) }.joined())
   list("E:/chameleon/slots")
  } else if b[0] == 0x16 {
   print("Directory bytes: " + body.map { String(format:"%02x",$0) }.joined())
   print("Directory text: " + String(decoding: body, as: UTF8.self))
   if stage == 0 { stage = 1; list("E:/chameleon") }
   else { finish("Read-only directory probe complete") }
  } else { finish("Unexpected response") }
 }

}
let probe = Probe()
probe.start()
let deadline = Date().addingTimeInterval(180)
while !probe.done && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.1)) }
if !probe.done { probe.finish("Timed out waiting for Pixl") }

exit(probe.success ? 0 : 1)
