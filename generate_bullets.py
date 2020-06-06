import json
import random

def randStartPos():
	return random.randint(300, 400)

def randStartVel():
	return random.randint(-100, 100)


points = [{"start": {"x": randStartPos(), "y": randStartPos()}, "velocity": {"x": randStartVel(), "y": randStartVel()}} for i in range(0, 10)]

j = json.dumps(points)

f = open("bullets.json", "w")
f.write(j)
f.close()
