"""OS credential store. Never falls back to a plaintext credential file."""
import ctypes
import json
import os
import sys
from ida_core import IdaError


class CredentialStore:
    def __init__(self, backend='system'):
        self.backend = backend
        self.keyring = None
        if backend == 'env':
            return
        if sys.platform == 'win32':
            from ctypes import wintypes as w

            class Credential(ctypes.Structure):
                _fields_ = [('Flags', w.DWORD), ('Type', w.DWORD), ('TargetName', w.LPWSTR),
                            ('Comment', w.LPWSTR), ('LastWritten', w.FILETIME),
                            ('CredentialBlobSize', w.DWORD), ('CredentialBlob', ctypes.POINTER(ctypes.c_ubyte)),
                            ('Persist', w.DWORD), ('AttributeCount', w.DWORD), ('Attributes', ctypes.c_void_p),
                            ('TargetAlias', w.LPWSTR), ('UserName', w.LPWSTR)]

            self.Credential = Credential
            self.api = ctypes.WinDLL('Advapi32.dll', use_last_error=True)
            self.api.CredWriteW.argtypes = [ctypes.POINTER(Credential), w.DWORD]
            self.api.CredReadW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.POINTER(ctypes.POINTER(Credential))]
            self.api.CredDeleteW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD]
            self.api.CredFree.argtypes = [ctypes.c_void_p]
            self.api.CredFree.restype = None
        else:
            try:
                import keyring
                # Select native secure backends explicitly, never keyrings.alt/plaintext.
                if sys.platform == 'darwin':
                    from keyring.backends.macOS import Keyring
                else:
                    from keyring.backends.SecretService import Keyring
                self.keyring = Keyring()
                if self.keyring.priority <= 0:
                    raise RuntimeError('Unavailable native keyring')
            except Exception:
                raise IdaError('CREDENTIAL_STORE_UNAVAILABLE', 'Install Python keyring and unlock the OS keychain, or use --credential-store env with process-injected credentials.')

    @staticmethod
    def target(ref):
        return 'ida-agent-client/' + ref

    def get(self, ref):
        if self.backend == 'env':
            data = {'clientId': os.getenv('IDA_CLIENT_ID'), 'clientSecret': os.getenv('IDA_CLIENT_SECRET'), 'proxyUser': os.getenv('IDA_PROXY_USER')}
            if not data['clientId'] or not data['clientSecret']:
                raise IdaError('CREDENTIALS_MISSING', 'Inject IDA_CLIENT_ID and IDA_CLIENT_SECRET into this process; do not put values in command text or files.')
            return {k: v for k, v in data.items() if v}
        if self.keyring is not None:
            value = self.keyring.get_password('ida-agent-client', ref)
        else:
            pointer = ctypes.POINTER(self.Credential)()
            if not self.api.CredReadW(self.target(ref), 1, 0, ctypes.byref(pointer)):
                code = ctypes.get_last_error()
                raise IdaError('CREDENTIAL_READ_FAILED', 'Credential unavailable in this OS login context; use the account used for initialization.', os_code=code)
            try:
                value = ctypes.string_at(pointer.contents.CredentialBlob, pointer.contents.CredentialBlobSize).decode('utf-8')
            finally:
                self.api.CredFree(pointer)
        if not value:
            raise IdaError('CREDENTIALS_MISSING', 'Initialize credentials for this profile.')
        return json.loads(value)

    def put(self, ref, data):
        if self.backend == 'env':
            return
        value = json.dumps(data, ensure_ascii=False)
        if self.keyring is not None:
            self.keyring.set_password('ida-agent-client', ref, value)
            if self.get(ref) != data:
                raise IdaError('CREDENTIAL_READBACK_FAILED', 'OS credential readback did not match.')
            return
        raw = value.encode('utf-8')
        if len(raw) > 2560:
            raise IdaError('CREDENTIAL_TOO_LARGE', 'Credential exceeds Windows native storage limit.')
        blob = (ctypes.c_ubyte * len(raw)).from_buffer_copy(raw)
        cred = self.Credential(Type=1, TargetName=self.target(ref), UserName='ida-agent-client',
                               CredentialBlobSize=len(raw), CredentialBlob=blob, Persist=2)
        if not self.api.CredWriteW(ctypes.byref(cred), 0):
            raise IdaError('CREDENTIAL_WRITE_FAILED', 'Could not save the credential in the OS store.', os_code=ctypes.get_last_error())
        if self.get(ref) != data:
            raise IdaError('CREDENTIAL_READBACK_FAILED', 'OS credential readback did not match.')

    def delete(self, ref):
        if self.backend == 'env':
            return
        if self.keyring is not None:
            self.keyring.delete_password('ida-agent-client', ref)
        elif not self.api.CredDeleteW(self.target(ref), 1, 0) and ctypes.get_last_error() != 1168:
            raise IdaError('CREDENTIAL_DELETE_FAILED', 'Could not remove the old OS credential.')
