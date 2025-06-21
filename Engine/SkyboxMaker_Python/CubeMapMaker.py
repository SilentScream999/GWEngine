inpt_str = """// Front face (negative Z)
-1.0 -1.0 -1.0 0.0 1.0
1.0 -1.0 -1.0 1.0 1.0
1.0 1.0 -1.0 1.0 0.0
1.0 1.0 -1.0 1.0 0.0
-1.0 1.0 -1.0 0.0 0.0
-1.0 -1.0 -1.0 0.0 1.0

// Back face (positive Z)
-1.0 -1.0 1.0 1.0 1.0
1.0 -1.0 1.0 0.0 1.0
1.0 1.0 1.0 0.0 0.0
1.0 1.0 1.0 0.0 0.0
-1.0 1.0 1.0 1.0 0.0
-1.0 -1.0 1.0 1.0 1.0

// Left face (negative X)
-1.0 1.0 1.0 0.0 0.0
-1.0 1.0 -1.0 1.0 0.0
-1.0 -1.0 -1.0 1.0 1.0
-1.0 -1.0 -1.0 1.0 1.0
-1.0 -1.0 1.0 0.0 1.0
-1.0 1.0 1.0 0.0 0.0

// Right face (positive X)
1.0 1.0 1.0 1.0 0.0
1.0 1.0 -1.0 0.0 0.0
1.0 -1.0 -1.0 0.0 1.0
1.0 -1.0 -1.0 0.0 1.0
1.0 -1.0 1.0 1.0 1.0
1.0 1.0 1.0 1.0 0.0

// Bottom face (negative Y)
-1.0 -1.0 -1.0 0.0 0.0
1.0 -1.0 -1.0 1.0 0.0
1.0 -1.0 1.0 1.0 1.0
1.0 -1.0 1.0 1.0 1.0
-1.0 -1.0 1.0 0.0 1.0
-1.0 -1.0 -1.0 0.0 0.0

// Top face (positive Y)
-1.0 1.0 -1.0 0.0 1.0
1.0 1.0 -1.0 1.0 1.0
1.0 1.0 1.0 1.0 0.0
1.0 1.0 1.0 1.0 0.0
-1.0 1.0 1.0 0.0 0.0
-1.0 1.0 -1.0 0.0 1.0"""

# Here we are going to change the numbers programatically just once

positions = [
	("Left", (1, 0)),
	("Front", (1, 1)),
	("Right", (1, 2)),
	("Back", (1, 3)),
	("Top", (0, 1)),
	("Bottom", (2, 1))
]

for section in inpt_str.split('\n\n'):
	lines = section.splitlines()
	
	newlines = [lines[0]]
	lines = lines[1:] # get rid of comments
	
	for ps in positions:
		if ps[0] in newlines[0]: # find the name
			pos = ps[1]
			break
			
	
	row, col = pos
	
	for line in lines:
		nums = [float(i) for i in line.split(' ')]
		newnums = nums[:3]
		uv = nums[-2:]
		
		# positions (newnums) and 0-1 for the individual squares.
		
		# offset by row/col
		newnums.append((uv[0] + col)/4) #x
		newnums.append((uv[1] + row)/3) #y 
		
		newlines.append(',\t'.join([str(i) for i in newnums]))
	
	print(',\n'.join(newlines)+',\n\n')

# Here we can create skyboxes

import os
import cv2
import sys
import numpy as np

nms = ['Left', 'Front', 'Right', 'Back', 'Top', 'Bottom']
imgs = [None for i in range(6)]
done = [False for i in range(6)]

for file in os.listdir('Skybox'):
	flnm = 'Skybox/'+file
	
	img = cv2.imread(flnm);
	
	for i in range(len(nms)):
		if nms[i].lower() in file.lower():
			imgs[i] = img
			done[i] = True
			break

if False in done:
	print('Missing images?')
	sys.exit()

w,h,_ = imgs[0].shape

for indx, img in enumerate(imgs):
	w1,h1,_ = img.shape
	if (w != w1 or h != h1):
		print(f'Mismatched sizes: {w}x{h} to {w1}x{h1} - having to resize')
		img = cv2.resize(img, (w, h))
		imgs[indx] = img

cubemap = np.zeros((h*3, w*4, 3))

for ps, img in zip(positions, imgs):
	r,c = ps[1]
	cubemap[r*h:(r+1)*h, c*w:(c+1)*w, 0:3] = img

cv2.imwrite('output_cubemap.bmp', cubemap.astype(np.uint8))