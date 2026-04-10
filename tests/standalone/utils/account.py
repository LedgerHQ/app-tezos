# Copyright 2023 Functori <contact@functori.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Module providing an account interface."""

from enum import IntEnum
from typing import Union

import coincurve
from coincurve import ecdsa as coincurve_ecdsa
import fastecdsa.curve
import fastecdsa.ecdsa
import fastecdsa.encoding.sec1
import nacl.exceptions
import nacl.signing
from ragger.bip import pack_derivation_path

from .message import Message
from .tezos_encoding import base58_decode, base58_encode, blake2b_32

class SigType(IntEnum):
    """Class representing signature type."""

    ED25519       = 0x00
    SECP256K1     = 0x01
    SECP256R1     = 0x02
    BIP32_ED25519 = 0x03

    def __str__(self) -> str:
        return self.name

class Signature(bytes):
    """Class representing signature."""

    def __str__(self) -> str:
        return self.decode()

    @staticmethod
    def from_secp256_tlv(tlv: Union[bytes, bytearray]) -> bytes:
        """Get the signature encapsulated in a TLV."""
        # See:
        # https://developers.ledger.com/docs/embedded-app/crypto-api/lcx__ecdsa_8h/#cx_ecdsa_sign
        # TLV: 30 || L || 02 || Lr || r || 02 || Ls || s
        if isinstance(tlv, bytes):
            tlv = bytearray(tlv)
        header_tag_index = 0
        # Remove the unwanted parity information set here.
        tlv[header_tag_index] &= ~0x01
        if tlv[header_tag_index] != 0x30:
            raise ValueError("Invalid TLV tag")
        len_index = 1
        if tlv[len_index] != len(tlv) - 2:
            raise ValueError("Invalid TLV length")
        first_tag_index = 2
        if tlv[first_tag_index] != 0x02:
            raise ValueError("Invalid TLV tag")
        r_len_index = 3
        r_index = 4
        r_len = tlv[r_len_index]
        second_tag_index = r_index + r_len
        if tlv[second_tag_index] != 0x02:
            raise ValueError("Invalid TLV tag")
        s_len_index = second_tag_index + 1
        s_index = s_len_index + 1
        s_len = tlv[s_len_index]
        r = tlv[r_index : r_index + r_len]
        s = tlv[s_index : s_index + s_len]
        # Sometimes \x00 are added or removed
        # A size adjustment is required here.
        def adjust_size(data, size):
            return data[-size:].rjust(size, b'\x00')
        return adjust_size(r, 32) + adjust_size(s, 32)

    @classmethod
    def from_bytes(cls, data: bytes, sig_type: SigType) -> 'Signature':
        """Get the signature according to the SigType."""
        if sig_type in {SigType.ED25519, SigType.BIP32_ED25519}:
            prefix = b'edsig'
        elif sig_type == SigType.SECP256K1:
            prefix = b'spsig'
            data = Signature.from_secp256_tlv(data)
        elif sig_type == SigType.SECP256R1:
            prefix = b'p2sig'
            data = Signature.from_secp256_tlv(data)
        else:
            assert False, f"Wrong signature type: {sig_type}"

        return cls(base58_encode(data, prefix))


class PublicKey(bytes):
    """Class representing public key."""

    def __str__(self) -> str:
        return self.decode()

    class CompressionKind(IntEnum):
        """Bytes compression kind"""
        EVEN         = 0x02
        ODD          = 0x03
        UNCOMPRESSED = 0x04

        def __bytes__(self) -> bytes:
            return bytes([self])

    @classmethod
    def from_bytes(cls, data: bytes, sig_type: Union[SigType, int]) -> 'PublicKey':
        """Convert a public key from bytes to string"""

        length, data = data[0], data[1:]
        assert length == len(data), f"Wrong data size, {length} != {len(data)}"

        # `data` should be:
        # kind + pk
        # pk length = 32 for compressed, 64 for uncompressed
        kind = data[0]
        data = data[1:]

        # Ed25519
        if sig_type in [
                SigType.ED25519,
                SigType.BIP32_ED25519
        ]:
            assert kind == cls.CompressionKind.EVEN, \
                f"Wrong Ed25519 public key compression kind: {kind}"
            assert len(data) == 32, \
                f"Wrong Ed25519 public key length: {len(data)}"
            return cls(base58_encode(data, b'edpk'))

        # Secp256
        if sig_type in [
                SigType.SECP256K1,
                SigType.SECP256R1
        ]:
            assert kind == cls.CompressionKind.UNCOMPRESSED, \
                f"Wrong Secp256 public key compression kind: {kind}"
            assert len(data) == 2 * 32, \
                f"Wrong Secp256 public key length: {len(data)}"
            kind = cls.CompressionKind.ODD if data[-1] & 1 else \
                cls.CompressionKind.EVEN
            prefix = b'sppk' if sig_type == SigType.SECP256K1 \
                else b'p2pk'
            data = bytes(kind) + data[:32]
            return cls(base58_encode(data, prefix))

        assert False, f"Wrong signature type: {sig_type}"


class AccountKey:
    """Class providing signature verification from a public key."""

    def __init__(self, encoded_public_key: str):
        self._encoded_public_key = encoded_public_key
        self._curve: bytes
        if encoded_public_key.startswith("edpk"):
            self._curve = b"ed"
        elif encoded_public_key.startswith("sppk"):
            self._curve = b"sp"
        elif encoded_public_key.startswith("p2pk"):
            self._curve = b"p2"
        else:
            raise ValueError(f"Unsupported public key prefix: {encoded_public_key[:4]}")
        self._public_point = base58_decode(encoded_public_key.encode())

    def public_key(self) -> str:
        """Return the encoded public key."""
        return self._encoded_public_key

    def verify(self, signature: Union[str, bytes], message: Union[str, bytes]) -> bool:
        """Verify that signature is a valid signature of message for this account."""
        encoded_signature = signature if isinstance(signature, bytes) else signature.encode()
        encoded_message = message if isinstance(message, bytes) else message.encode()

        if encoded_signature[:3] != b"sig":
            if self._curve != encoded_signature[:2]:
                raise ValueError("Signature and public key curves mismatch.")

        decoded_signature = base58_decode(encoded_signature)

        if self._curve == b"ed":
            digest = blake2b_32(encoded_message).digest()
            try:
                nacl.signing.VerifyKey(self._public_point).verify(digest, decoded_signature)
            except nacl.exceptions.BadSignatureError as exc:
                raise ValueError("Signature is invalid.") from exc
        elif self._curve == b"sp":
            pk = coincurve.PublicKey(self._public_point)
            der_sig = coincurve_ecdsa.cdata_to_der(
                coincurve_ecdsa.deserialize_compact(decoded_signature)
            )
            if not pk.verify(
                signature=der_sig,
                message=encoded_message,
                hasher=lambda x: blake2b_32(x).digest(),
            ):
                raise ValueError("Signature is invalid.")
        elif self._curve == b"p2":
            pk = fastecdsa.encoding.sec1.SEC1Encoder.decode_public_key(
                self._public_point, curve=fastecdsa.curve.P256
            )  # type: ignore[assignment]
            r = int.from_bytes(decoded_signature[:32], "big")
            s = int.from_bytes(decoded_signature[32:], "big")
            if not fastecdsa.ecdsa.verify(
                    sig=(r, s), msg=encoded_message, Q=pk, hashfunc=blake2b_32):  # type: ignore[arg-type]
                raise ValueError("Signature is invalid.")
        else:
            raise ValueError(f"Invalid or unsupported curve type: `{self._curve!r}`.")

        return True


class Account:
    """Class representing account."""

    path: bytes
    sig_type: Union[SigType, int]
    __key: str

    def __init__(self,
                 path: Union[str, bytes],
                 sig_type: Union[SigType, int],
                 key: str):
        self.path = \
            pack_derivation_path(path) if isinstance(path, str) \
            else path
        self.sig_type = sig_type
        self.__key = key

    def __repr__(self) -> str:
        return self.__key

    @property
    def key(self) -> AccountKey:
        """Public key wrapper providing signature verification."""
        return AccountKey(self.__key)

    def check_signature(
            self,
            data: bytes,
            message: Message,
            with_hash: bool):
        """Checks if signature correspond to a signature of message sign by the account."""
        if with_hash:
            assert data.startswith(message.hash), \
                f"Expected a starting hash {message.hash.hex()} but got {data.hex()}"
            data = data[len(message.hash):]

        signature = Signature.from_bytes(data, SigType(self.sig_type))

        assert self.key.verify(signature, bytes(message)), \
            f"Fail to verify signature {signature!r}, \n\
            with account {self} \n\
            and message {message}"


DEFAULT_SEED = ' '.join(['zebra']*24)

DEFAULT_ACCOUNT = Account(
    "m/44'/1729'/0'/0'",
    SigType.ED25519,
    "edpkuXX2VdkdXzkN11oLCb8Aurdo1BTAtQiK8ZY9UPj2YMt3AHEpcY"
)
