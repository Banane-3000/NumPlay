"""How the vehicles and Bill are drawn over their bodies, as the game draws them.

The game puts each picture on its physics body, centred and stretched to a size in meters (its PlayerCar*::create
and PlayerCar::createDriver call PhysicsSimulation::createSprite(picture, body, width, height)): the chassis'
picture on the chassis, a wheel's on the wheel (2.15 times its radius square), Bill's head on his head (0.96 m).
The wiki keeps the same pictures (the same 256 x 128 chassis canvases). tools/pack.py turns this into src/art.h
(pictures at VPPM pixels a meter, pivots at their centres) and tools/preview.py draws each vehicle put together."""

# Each vehicle: its chassis picture and the size the game draws it (meters, centred on the chassis body); the
# wheel's picture and the radius its 2.15 r square is drawn for (or for a cropped tyre: its radius in pixels);
# Bill's legs on the bikes; tubes from chassis points (meters) to an axle (fork, swingarm): colour, radius;
# extra pictures on the chassis: (picture, size, centre (meters), in front)
VEHICLES = [
    dict(id="jeep", name="Hill Climber", chassis="Chassis-hillclimber-red.png", size=(3.657, 1.829),
         wheel="Common_Wheel_-_Type_1.png", wheel_r=0.4),
    dict(id="bike", name="Motocross Bike", chassis="Bike-motocross.png", size=(3.2, 1.6),
         wheel="Bike-motocross-tire.png", wheel_r=0.5, legs=True,
         tubes=[((0.0, -0.5), 0, 0x5A5E66, 0.09), ((0.6, 0.425), 1, 0x30343A, 0.07)]),
    dict(id="monster", name="Monster Truck", chassis="Car-monster.png", size=(4.267, 2.133),
         wheel="Car-monster-tire.png", wheel_r=0.7),
    dict(id="quad", name="Quad Bike", chassis="Car-quadbike.png", size=(3.413, 1.707),
         wheel="Common_Wheel_-_Type_4.png", wheel_r=0.6, legs=True),
    dict(id="race", name="Race Car", chassis="Car-racecar.png", size=(5.689, 2.844),
         wheel="Common_Wheel_-_Type_2.png", wheel_r=0.5),
    dict(id="rally", name="Rally Car", chassis="Car-rallycar.png", size=(4.376, 2.188),
         wheel="Common_Wheel_-_Type_8.png", wheel_r=0.5),
    dict(id="buggy", name="Dune Buggy", chassis="Dune_Buggy_Chassis_(Sprite).png", size=(5.12, 2.56),
         wheel="buggy-tire", wheel_r=0.5, wheel_px=39),
    dict(id="tank", name="Tank", chassis="Car-tank-fg.png", size=(5.689, 2.56),
         wheel="Car-tank-tire.png", wheel_r=0.35,
         layers=[("Car-tank-bg.png", (5.689, 2.56), (0, 0), False), ("Car-tank-cannon.png", (2.327, 1.164), (1.6, 0.8), True)]),
]

# Bill: his head is drawn on his head (a circle of 0.33 m) as wide as the game draws it (0.855 m: 57 pixels of its
# 0.96 m square picture of 64); his body from his hips (the bottom of his torso, a 0.6 m box) to his neck (its top):
# the body picture's hips and neck pixels; his legs on bikes (the leg picture, its hip, its height)
BILL = dict(head="Driver-head.png", head_m=0.855, body="Driver-body.png", body_neck=(34, 10), body_hips=(36, 110),
            torso_m=0.6, leg="Bike-driver-leg-full.png", leg_hip=(12, 28), leg_m=0.78)
