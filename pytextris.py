import ctypes
import threading
import time
import pygame
import pygame.locals
import curses


def start(textris):
	textris.textris()

_textris = ctypes.CDLL('textris.so')

t = threading.Thread(target=start, args = (_textris,))
t.start()

pygame.init()

left = False
right = False

running = True

DAS = 0.15
ARR = 0.075

while t.isAlive():
	for event in pygame.event.get():
		if event.type == pygame.KEYDOWN:
			if event.key == pygame.K_LEFT:
				_textris.shape_left()
				movetime = time.time() + DAS
				left = True
			elif event.key == pygame.K_RIGHT:
				_textris.shape_right()
				movetime = time.time() + DAS
				right = True
			elif event.key == pygame.K_SPACE:
				_textris.shape_hard_drop()
			elif event.key == pygame.K_UP:
				_textris.shape_rotate()
			elif event.key == pygame.K_LSHIFT:
				_textris.shape_hold()
			elif event.key == pygame.K_DOWN:
				_textris.drop_speed_fast()

		elif event.type == pygame.KEYUP:
			if event.key == pygame.K_LEFT:
				left = False
			elif event.key == pygame.K_RIGHT:
				right = False
			elif event.key == pygame.K_DOWN:
				_textris.drop_speed_normal()
	
	if left == True and (time.time() - movetime) > ARR:
		movetime = time.time()
		_textris.shape_left()
	if right == True and (time.time() - movetime) > ARR:
		movetime = time.time()
		_textris.shape_right()

pygame.quit()
