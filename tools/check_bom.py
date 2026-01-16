from pathlib import Path
p=Path('c:/Users/T14/Documents/PlatformIO/Projects/prueba 4/.github/workflows/deploy-pages.yml')
b=p.read_bytes()
print('length=',len(b))
print('first4=',b[:4])
print('repr first160=')
print(repr(b[:160]))
