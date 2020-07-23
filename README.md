﻿# BulletsManager

This is a toy problem.

There are walls on a 2D plane, represented by two points.
There are bullets, represented by their starting point, direction and speed, fire time and lifetime.

The list of walls is predefined but bullets may be added dynamically throughout the simulation.

When a bullet collides with a wall, the wall should be destroyed and the bullet's velocity must be reflected.

The task is to write such a simulation and a visualization for it.

The solution should use all available threads/cores.

## TODO

* Loading the set of wall from a file ✔
* Pausing and continuing the simulation
* Navigation on the simulation field
* Spatial partitioning
* Implement a threadpool, check performance ✔ (about a third faster)
* Add caching for bullet collisions so that results from step 1 could be used in step 2
