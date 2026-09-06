import Foundation
import CryptoKit
let base = URL(fileURLWithPath: CommandLine.arguments[1])
let key = try P256.Signing.PublicKey(x963Representation: Data(contentsOf: base.appendingPathComponent("public-key.bin")))
let message = try Data(contentsOf: base.appendingPathComponent("signed-message.bin"))
let signature = try P256.Signing.ECDSASignature(rawRepresentation: Data(contentsOf: base.appendingPathComponent("signature.bin")))
let valid = key.isValidSignature(signature, for: message)
print("Signature valid against upstream Pixl public key: \(valid)")
if !valid { exit(1) }
