import sys, glob, os
from PIL import Image, ImageDraw
d = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 48
start = int(sys.argv[3]) if len(sys.argv) > 3 else 0
end = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
fs = sorted(f for f in glob.glob(d + '/frames/*.pgm') if start <= int(os.path.basename(f)[:-4]) <= end)
if not fs: sys.exit('no frames')
step = max(1, len(fs) // n); fs = fs[::step][:n]
cols = 8; rows = (len(fs) + cols - 1) // cols
sheet = Image.new('L', (cols * 160, rows * 144), 0)
for i, f in enumerate(fs):
    im = Image.open(f); ImageDraw.Draw(im).text((2, 2), os.path.basename(f)[:-4].lstrip('0') or '0', fill=0)
    sheet.paste(im, ((i % cols) * 160, (i // cols) * 144))
out = sys.argv[5] if len(sys.argv) > 5 else d + '/sheet.png'
sheet.save(out); print(out, len(fs), 'frames')
