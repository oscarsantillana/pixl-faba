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

guard CommandLine.arguments.count == 2 else { print("Usage: pixl-read-faba-diag <new-report-directory>"); exit(2) }
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
 let files = ["config.bin", "00.bin", "01.bin", "02.bin"]
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
   send(0x03)
  } else if b[0] == 0x03 && b[1] == 0 && body.count == 64 {
   let keys = ["magic","flags","reset_reason","phase","story","start_tick","end_tick","last_input","input_tick","input_count","free_start","biggest_start","free_stop","fault_id","fault_pc","fault_info"]
   var report: [String: Any] = [:]
   for (i, key) in keys.enumerated() {
    let n = i * 4
    report[key] = UInt32(body[n]) | UInt32(body[n+1]) << 8 | UInt32(body[n+2]) << 16 | UInt32(body[n+3]) << 24
   }
   guard report["magic"] as? UInt32 == 0x37444246 else { finish("Unrecognized diagnostic format"); return }
   do {
    try Data(body).write(to: outputDirectory.appendingPathComponent("report.bin"))
    let json = try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys])
    try json.write(to: outputDirectory.appendingPathComponent("report.json"))
    print(String(decoding: json, as: UTF8.self))
    success = true; finish("Read-only diagnostic saved")
   } catch { finish("Save failed: \(error)") }
  } else { finish("Unexpected response or unsupported diagnostic command") }
 }

}
let probe = Probe()
probe.start()
let deadline = Date().addingTimeInterval(35)
while !probe.done && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.1)) }
if !probe.done { probe.finish("Timed out waiting for Pixl") }

exit(probe.success ? 0 : 1)
