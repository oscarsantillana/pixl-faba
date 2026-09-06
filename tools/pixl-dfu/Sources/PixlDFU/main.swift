import Foundation
import CoreBluetooth
import NordicDFU

setbuf(stdout, nil)

// Transfer implementation is Nordic's unmodified library, pinned to release 4.17.0.
// This wrapper only selects an explicitly identified peripheral and reports results.
let args = CommandLine.arguments
func fail(_ message: String) -> Never { print(message); exit(1) }
guard args.count >= 2 else { fail("Usage: PixlDFU validate ZIP | scan | flash ZIP DFU-UUID") }
let mode = args[1]
guard ["validate", "scan", "flash"].contains(mode) else { fail("Unknown mode") }
var firmware: DFUFirmware?
if mode != "scan" {
    guard args.count == (mode == "flash" ? 4 : 3) else { fail("Invalid arguments") }
    do {
        firmware = try DFUFirmware(urlToZipFile: URL(fileURLWithPath: args[2]))
        guard firmware!.valid, firmware!.parts == 1 else { fail("Invalid or multipart package") }
        print("Nordic package parser: valid; parts=\(firmware!.parts)")
    } catch { fail("Package rejected: \(error)") }
}
if mode == "validate" { exit(0) }
let targetID: UUID? = mode == "flash" ? UUID(uuidString: args[3]) : nil
if mode == "flash", targetID == nil { fail("An exact DFU peripheral UUID is required") }

final class Updater: NSObject, CBCentralManagerDelegate, DFUServiceDelegate, DFUProgressDelegate, LoggerDelegate {
    var central: CBCentralManager!
    var initiator: DFUServiceInitiator?
    var controller: DFUServiceController?
    var started = false
    var done = false
    var success = false
    var seen = Set<UUID>()
    func start() { central = CBCentralManager(delegate: self, queue: .main) }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        print("Bluetooth state=\(central.state.rawValue)")
        if central.state == .poweredOn {
            central.scanForPeripherals(withServices: [CBUUID(string: "FE59")], options: nil)
        } else if [.unauthorized, .unsupported, .poweredOff].contains(central.state) {
            done = true
        }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "Unnamed"
        if seen.insert(peripheral.identifier).inserted {
            print("DFU peripheral: \(peripheral.identifier) name=\(name) RSSI=\(RSSI)")
        }
        guard mode == "flash", peripheral.identifier == targetID, !started else { return }
        started = true
        central.stopScan()
        print("Starting application transfer to explicitly selected UUID \(peripheral.identifier)")
        let service = DFUServiceInitiator(centralManager: central, target: peripheral).with(firmware: firmware!)
        service.delegate = self
        service.progressDelegate = self
        service.logger = self
        initiator = service
        controller = service.start()
        if controller == nil { print("Nordic DFU did not start"); done = true }
    }
    func dfuStateDidChange(to state: DFUState) {
        print("DFU state: \(state)")
        if state == .completed { success = true; done = true }
        if state == .aborted { done = true }
    }
    func dfuError(_ error: DFUError, didOccurWithMessage message: String) {
        print("DFU ERROR \(error.rawValue): \(message)")
        done = true
    }
    func dfuProgressDidChange(for part: Int, outOf totalParts: Int, to progress: Int,
                             currentSpeedBytesPerSecond: Double, avgSpeedBytesPerSecond: Double) {
        if progress % 5 == 0 { print("Upload \(progress)% (\(Int(avgSpeedBytesPerSecond)) B/s)") }
    }
    func logWith(_ level: LogLevel, message: String) {
        if level == .warning || level == .error || level == .application { print("DFU: \(message)") }
    }
}
let updater = Updater()
updater.start()
let deadline = Date().addingTimeInterval(mode == "scan" ? 30 : 600)
let scanDeadline = Date().addingTimeInterval(180)
while !updater.done && Date() < deadline {
    if mode == "flash", !updater.started, Date() > scanDeadline { break }
    RunLoop.current.run(until: Date().addingTimeInterval(0.1))
}
if !updater.started { updater.central.stopScan() }
if mode == "scan" { print("Read-only scan finished"); exit(0) }
if !updater.done { print("Timed out; transfer completion NOT confirmed") }
exit(updater.success ? 0 : 1)
