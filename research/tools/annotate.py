"""Print a locally decompiled function with component names substituted for the static DAT_0102xxxx pointers.
Needs the local-only decompiler output (research/decomp/functions) and dat_names.json (run dat_names.py first).
usage: python3 -I annotate.py <function address, e.g. 0x100c939> [decomp functions dir]"""
import json,os,re,sys
here=os.path.dirname(os.path.abspath(__file__))
names=json.load(open(os.path.join(here,"dat_names.json")))
fdir=sys.argv[2] if len(sys.argv)>2 else os.path.join(here,"..","decomp","functions")
src=open(os.path.join(fdir,sys.argv[1]+".c")).read()
src=src[src.index("// pseudocode"):].split("\n",1)[1]
def sub(m):
    a="0x"+m.group(1).lstrip("0"); return "<%s>"%names[a] if a in names else m.group(0)
print(re.sub(r"\*?\(?\(?DAT_0*(1[0-9a-f]{6})\)?",lambda m: sub(m),src) if False else re.sub(r"DAT_0(1[0-9a-f]{6})",lambda m: ("<%s>"%names["0x"+m.group(1)]) if ("0x"+m.group(1)) in names else m.group(0),src))
