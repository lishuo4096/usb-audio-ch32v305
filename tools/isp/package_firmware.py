"""Build first-install and application-only images from the supplied WCH ISP assets."""
import hashlib
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
app = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'obj/USBAudio.bin'
if len(sys.argv) <= 1:
    objcopy = Path('C:/MounRiver/MounRiver_Studio2/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC12/bin/riscv-wch-elf-objcopy.exe')
    subprocess.run([str(objcopy), '-O', 'binary', str(root / 'obj/USBAudio.elf'), str(app)], check=True)
out = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else root / 'firmware/isp'
rom = Path(__file__).parent / 'bootloader/ch32v30x_isp_rom_1FFF8000.bin'
assert hashlib.sha256(rom.read_bytes()).hexdigest() == '79a72fe03ba7bfc1a9853d3e1af2a575c31593647e5aab58e2f0c996078e3df9', 'ROM asset changed'
subprocess.run([sys.executable, str(Path(__file__).with_name('pack_bootloader.py')),
                '--chip', '305', '--app', str(app), '--rom', str(rom), '--out', str(out)], check=True)
blob = (out / 'bootloader_0801C000.bin').read_bytes()
assert hashlib.sha256(blob).hexdigest() == '7fefe40b921937c132447276d1c8ba9805015e98219eb2c12708f3df033a9dc2', 'Relocation mismatch'
first = (out / 'first_install.bin').read_bytes()
assert len(app.read_bytes()) <= 0x1C000 and first[0x1C000:] == blob
full = first.ljust(128 * 1024, b'\xff')
assert len(full) == 131072
(out / 'first_install_128k.bin').write_bytes(full)
manifest = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.bin')}
import json
(out / 'sha256.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print('Complete first-install image: 131072 B; subsequent ISP updates: app_only.bin')
