import ctypes
import threading
import time
import pygame
import pygame.locals
import curses


def start(textris):
	textris.start()

_textris = ctypes.CDLL('textris.so')

t = threading.Thread(target=start, args = (_textris,))
t.deamon = True
t.start()

time.sleep(2)

_textris.test_func()
_textris.shape_left()

pygame.init()

while True:
	for event in pygame.event.get():
		if event.type == pygame.KEYDOWN:
			if event.key == pygame.K_a:
				_textris.shape_left()
	
pygame.quit()
