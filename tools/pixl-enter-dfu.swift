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

guard CommandLine.arguments.count == 2 else { print("Usage: pixl-enter-dfu <expected-current-version>"); exit(2) }
let expectedVersion = CommandLine.arguments[1]
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
  if let e = error { finish("Response error: \(e)"); return }
  guard let data = c.value else { return }
  let b = [UInt8](data)
  guard b.count >= 4, b[1] == 0 else { finish("Invalid response"); return }
  let body = Array(b.dropFirst(4))
  if b[0] == 1, body.count >= 2 {
   let n = Int(body[0]) | Int(body[1]) << 8
   guard body.count >= n+2 else { finish("Short version response"); return }
   let version = String(decoding: body[2..<2+n], as: UTF8.self)
   guard version == expectedVersion else { finish("Unexpected firmware: " + version); return }
   print("Verified known Pixl UUID and firmware: " + version)
   print("Requesting firmware-update mode")
   stage = 1; send(0x02)
  } else if b[0] == 2, stage == 1 {
   print("DFU entry acknowledged")
   success = true
   DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { self.finish("DFU mode requested successfully") }
  } else { finish("Unexpected response") }
 }


}
let probe = Probe()
probe.start()
let deadline = Date().addingTimeInterval(35)
while !probe.done && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.1)) }
if !probe.done { probe.finish("Timed out waiting for Pixl") }

exit(probe.success ? 0 : 1)
