import json
import random

def rand():
	return random.randint(0, 1000)

points = [{"start": {"x": rand(), "y": rand()}, "end": {"x": rand(), "y": rand()}} for i in range(0, 10000)]

j = json.dumps(points)

f = open("walls.json", "w")
f.write(j)
f.close()
