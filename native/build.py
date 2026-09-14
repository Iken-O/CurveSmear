"""Build with the installed MSVC and Windows SDK; leaves Adobe SDK files untouched."""
import argparse, os, pathlib, shutil, subprocess

ROOT=pathlib.Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser()
p.add_argument('--sdk',default=r'C:\SDK\Adobe\AE_SDK\25.2\AfterEffectsSDK')
p.add_argument('--tests',action='store_true')
p.add_argument('--benchmark-only',action='store_true',help='Build the standalone CPU benchmark without rebuilding the plugin')
p.add_argument('--output',default='CurveSmear.aex',help='Output filename inside dist/')
args=p.parse_args()
sdk=pathlib.Path(args.sdk)/'Examples'
compiler=pathlib.Path(shutil.which('cl') or '')
if not compiler.is_file(): raise SystemExit('MSVC cl.exe must be on PATH')
msvc=compiler.parents[3]
kits=pathlib.Path(r'C:\Program Files (x86)\Windows Kits\10')
version=sorted((kits/'Include').glob('10.*'),key=lambda x:tuple(map(int,x.name.split('.'))))[-1].name
env=os.environ.copy()
env['INCLUDE']=';'.join(map(str,[msvc/'include',kits/'Include'/version/'ucrt',kits/'Include'/version/'shared',kits/'Include'/version/'um']))
env['LIB']=';'.join(map(str,[msvc/'lib'/'x64',kits/'Lib'/version/'ucrt'/'x64',kits/'Lib'/version/'um'/'x64']))
build=ROOT/'build';build.mkdir(exist_ok=True)
dist=ROOT/'dist';dist.mkdir(exist_ok=True)
plugin=dist/args.output
includes=[f'/I{sdk/x}' for x in ['Headers','Headers/SP','Headers/Win','Util','Resources']]
def run(argv,output=None):
    print('Running:',pathlib.Path(str(argv[0])).name,flush=True)
    if output:
        with open(output,'wb') as stream: subprocess.run(list(map(str,argv)),env=env,cwd=build,stdout=stream,check=True)
    else: subprocess.run(list(map(str,argv)),env=env,cwd=build,check=True)
if args.benchmark_only:
    run([compiler,'/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/D_CRT_SECURE_NO_WARNINGS','/DMSWindows','/D_WINDOWS',*includes,ROOT/'native/Benchmark.cpp',ROOT/'native/CurveSmearUI.cpp',f'/Fe:{build / "Benchmark.exe"}'])
    raise SystemExit(0)
run([compiler,'/nologo','/EP',*includes,ROOT/'native/CurveSmearPiPL.r'],build/'CurveSmear.rr')
run([sdk/'Resources/PiPLTool.exe',build/'CurveSmear.rr',build/'CurveSmear.rrc'])
run([compiler,'/nologo','/DMSWindows','/EP',build/'CurveSmear.rrc'],build/'CurveSmear.rc')
run([kits/'bin'/version/'x64/rc.exe','/nologo',f'/fo{build / "CurveSmear.res"}',build/'CurveSmear.rc'])
run([compiler,'/nologo','/std:c++17','/EHsc','/O2','/MT','/LD','/W4','/D_CRT_SECURE_NO_WARNINGS','/DMSWindows','/D_WINDOWS',*includes,ROOT/'native/CurveSmear.cpp',ROOT/'native/CurveSmearUI.cpp',build/'CurveSmear.res','/link',f'/OUT:{plugin}'])
print('Built:',plugin)
if args.tests:
    run([compiler,'/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/D_CRT_SECURE_NO_WARNINGS','/DMSWindows','/D_WINDOWS',*includes,ROOT/'native/CoreTests.cpp',ROOT/'native/CurveSmearUI.cpp',f'/Fe:{build / "CoreTests.exe"}'])
    run([build/'CoreTests.exe'])
