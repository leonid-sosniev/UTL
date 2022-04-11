#!/usr/bin/python3

import sys, subprocess, os, glob, time

def help():
    print("Synopsis:")
    print("\tThisApp [-i IncludeDirsGlobsCommaList] SourceFileGlobsCommaList ResultDir TargetName")
    print("")
    print("IncludeDirsCommaList -- list of comma-separated header-containing dirs paths")
    print("SourceFilesCommaList -- list of comma-separated source file paths")
    print("ResultDir            -- where to store resulting binaries")
    print("TargetName           -- name of output binary")
    exit(1)

def isOption(s: str):
    return len(s) == 2 and s.startswith('-')

def parseGlobCommaList(argvItem: str):
    result = list()
    for globPattern in argvItem.split(','):
        globPattern = globPattern.strip()
        if (len(globPattern) == 0):
            continue
        for filePath in glob.glob(globPattern, recursive = True):
            result.append(filePath)
    return result

print("=" * 80)
print("CMD ARGS:", sys.argv)
print("=" * 80)

TargetName = ""
ResultDir = ""
SourceFilesCommaList = []
IncludeDirsCommaList = []
i = 1

try:
    print("ITERATING THE OPTIONS")
    while isOption(sys.argv[i]):
        if sys.argv[i] == "-i":
            i += 1
            if len(sys.argv)>i and sys.argv[i]:
                IncludeDirsCommaList += parseGlobCommaList(sys.argv[i])
                print("INCLUDES: ", IncludeDirsCommaList)
            else:
                print("EXPECTED AN OPTION")
                help()
                exit() # ====================================================== EXIT ====
        else:
            print("UNDEFINED OPTION", sys.argv[i])
            help()
            exit() # ========================================================== EXIT ====
        i += 1

    print("PARSING SOURCE FILE GLOBS")
    if len(sys.argv)>i and sys.argv[i]:
        SourceFilesCommaList += parseGlobCommaList(sys.argv[i])
        print("SOURCES", SourceFilesCommaList)
    i += 1

    ResultDir = os.path.abspath(sys.argv[i])
    print("ABSOLUTE TARGET DIR PATH: ", ResultDir)
    i += 1

    TargetName = sys.argv[i]
    print("TARGET EXECUTABLE NAME:", TargetName)
    i += 1

except Exception as exc:
    print(exc)
    help()

print("=" * 80)
# add basic options
options = [ "g++", "-m64", "-o", os.path.join(ResultDir,TargetName), "-x", "c++", "-g", "-std=c++14", "-pedantic", "-pipe", "-pthread", "-B", "/usr/local/include", "-Wall", "-Wextra" ]
# add include dir paths
for path in IncludeDirsCommaList: options += [ "-I%s"%path ]
# add source files *
for path in SourceFilesCommaList: options += [ "-v", path  ]
# add static GCC libraries
options += [ "-lrt", "-lstdc++fs", "-lcrypto", "-pipe", "-pthread" ]

print("=" * 80)
print("OPTIONS TO COMPILER:", options)
print("=" * 80)
time.sleep(1)

# run build
exitCode = subprocess.call(options)
if exitCode != 0:
    print("="*80, "NONZERO EXIT CODE:", exitCode)
else:
    print("="*80, "SUCCESS")
