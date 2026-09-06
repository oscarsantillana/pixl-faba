import Foundation
import AppKit
import CoreGraphics

// Compile FABA's official small logo into the display's 32px, one-bit XBM format.
let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let source = root.appendingPathComponent("assets/faba/official-favicon.png")
let input = NSImage(contentsOf: source)!
var rect = CGRect(x: 0, y: 0, width: input.size.width, height: input.size.height)
let cg = input.cgImage(forProposedRect: &rect, context: nil, hints: nil)!
var rgba = [UInt8](repeating: 255, count: 32 * 32 * 4)
rgba.withUnsafeMutableBytes { bytes in
 let ctx = CGContext(data: bytes.baseAddress, width: 32, height: 32, bitsPerComponent: 8,
  bytesPerRow: 128, space: CGColorSpaceCreateDeviceRGB(),
  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
 ctx.setFillColor(CGColor(gray: 1, alpha: 1)); ctx.fill(CGRect(x: 0, y: 0, width: 32, height: 32))
 ctx.interpolationQuality = .high
 ctx.draw(cg, in: CGRect(x: 0, y: 0, width: 32, height: 32))
}
var bitmap = [UInt8](repeating: 0, count: 128)
for y in 0..<32 {
 for x in 0..<32 {
  let p = (y * 32 + x) * 4
  let luminance = (Int(rgba[p]) * 299 + Int(rgba[p+1]) * 587 + Int(rgba[p+2]) * 114) / 1000
  if luminance < 170 { bitmap[y * 4 + x / 8] |= 1 << (x % 8) }
 }
}
var header = "// Compiled from the official FABA favicon; provenance in assets/faba/source.json.\n#ifndef FABA_ICON_H\n#define FABA_ICON_H\n#include \"mui_resource.h\"\nstatic const uint8_t faba_icon_data[] = {\n"
for i in stride(from: 0, to: 128, by: 16) {
 header += "    " + bitmap[i..<i+16].map { String(format: "0x%02x", $0) }.joined(separator: ",") + ",\n"
}
header += "};\nstatic const xbm_t app_faba_32x32 = {32, 32, faba_icon_data};\n#endif\n"
try header.write(to: root.appendingPathComponent("firmware/pixl-faba/fw/application/src/mod/faba_icon.h"), atomically: true, encoding: .utf8)
// Enlarge exact output pixels for inspection, without interpolation.
let preview = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 256, pixelsHigh: 256,
 bitsPerSample: 8, samplesPerPixel: 3, hasAlpha: false, isPlanar: false,
 colorSpaceName: .deviceRGB, bytesPerRow: 768, bitsPerPixel: 24)!
for y in 0..<256 { for x in 0..<256 {
 let bit = bitmap[(y / 8) * 4 + (x / 8) / 8] & (1 << ((x / 8) % 8))
 let value: UInt8 = bit == 0 ? 255 : 0
 let offset = y * 768 + x * 3
 for channel in 0..<3 { preview.bitmapData![offset+channel] = value }
}}
try preview.representation(using: .png, properties: [:])!.write(to: root.appendingPathComponent("out/faba-logo-icon.png"))
print("Compiled official FABA logo to 128-byte XBM and exact-pixel preview")
