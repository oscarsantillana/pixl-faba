import Foundation
import CoreBluetooth
setbuf(stdout, nil)
final class Scanner: NSObject, CBCentralManagerDelegate {
 var manager: CBCentralManager!
 var seen = Set<UUID>()
 var failed = false
 func start() { manager = CBCentralManager(delegate: self, queue: .main) }
 func centralManagerDidUpdateState(_ central: CBCentralManager) {
  if central.state == .poweredOn {
   central.scanForPeripherals(withServices: [CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"), CBUUID(string: "FE59")], options: nil)
  } else if [.unauthorized, .unsupported, .poweredOff].contains(central.state) {
   print("Bluetooth unavailable: \(central.state.rawValue)"); failed = true
  }
 }
 func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
  guard seen.insert(peripheral.identifier).inserted else { return }
  let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "Unnamed"
  let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
  print("\(peripheral.identifier) name=\(name) RSSI=\(RSSI) services=\(services)")
 }
}
let scanner = Scanner(); scanner.start()
let deadline = Date().addingTimeInterval(30)
while !scanner.failed && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.1)) }
scanner.manager.stopScan()
print("Read-only scan finished. No connection or write attempted.")
exit(scanner.failed ? 1 : 0)
