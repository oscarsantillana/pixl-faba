// swift-tools-version:5.9
import PackageDescription
import Foundation
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()

let package = Package(
    name: "PixlDFU",
    platforms: [.macOS(.v13)],
    dependencies: [.package(path: "../../.deps/IOS-DFU-Library")],
    targets: [.executableTarget(
        name: "PixlDFU",
        dependencies: [.product(name: "NordicDFU", package: "IOS-DFU-Library")],
        linkerSettings: [.unsafeFlags([
            "-Xlinker", "-sectcreate", "-Xlinker", "__TEXT",
            "-Xlinker", "__info_plist", "-Xlinker", root.appendingPathComponent("docs/toolchain/Info.plist").path
        ])]
    )]
)
