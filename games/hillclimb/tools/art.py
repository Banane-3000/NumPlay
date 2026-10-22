"""How the vehicles and Bill are drawn over their bodies, as the game draws them.

The game puts each picture on its physics body, centred and stretched to a size in meters (its PlayerCar*::create
and PlayerCar::createDriver call PhysicsSimulation::createSprite(picture, body, width, height)): the chassis'
picture on the chassis, a wheel's on the wheel (2.15 times its radius square), Bill's head on his head (0.96 m).
The wiki keeps the same pictures (the same 256 x 128 chassis canvases). tools/pack.py turns this into src/art.h
(pictures at VPPM pixels a meter, pivots at their centres) and tools/preview.py draws each vehicle put together."""

# Each vehicle: its chassis picture and the size the game draws it (meters, centred on the chassis body); the
# wheel's picture and the radius its 2.15 r square is drawn for (or for a cropped tyre: its radius in pixels; or
# wheel_side x r; or wheel_reuse: another vehicle's tyre picture, drawn for wheel_r); Bill's legs on the bikes;
# tubes from chassis points (meters) to an axle (fork, swingarm): colour, radius; extra pictures on the chassis:
# (picture, size, centre (meters), in front); pictures the game puts behind the chassis' (under: picture, its centre
# in the chassis picture's pixels); trim: their clear margins cut (the pivot kept). "game/" pictures are the game's
# own files (tools/fetch.py).
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
    # 1.10.2's other five (PlayerCarTractor, PlayerCarTouristbus, PlayerCarDieselTruck, PlayerCarSnowMobile,
    # PlayerCarTruck::create). The tractor's tyre is drawn 2 r square, the soft back one and the small front one
    # alike; its steering wheel is a child of the chassis' picture at (260, 210) from its bottom left, behind it
    dict(id="tractor", name="Tractor", chassis="game/car-tractor.png", size=(6.69257, 3.34629),
         wheel="game/car-tractor-tire.png", wheel_r=1.09, wheel_side=2.0, trim=True,
         under=[("game/car-tractor-steering-wheel.png", (260, 256 - 210))]),
    dict(id="bus", name="Tourist Bus", chassis="game/car-touristbus.png", size=(8.46281, 4.2314),
         wheel_reuse="monster", wheel_r=0.7, trim=True),
    dict(id="diesel", name="Super Diesel 4x4", chassis="game/car-pickup.png", size=(5.50538, 2.75269),
         wheel_reuse="monster", wheel_r=0.7, trim=True),
    # (the snow mobile's track wheels: the tank's tyre 0.5474 m across for r 0.336)
    dict(id="snowmobile", name="Snow Mobile", chassis="Car-snowmobile-body.png", size=(5.68889, 2.56),
         wheel_reuse="tank", wheel_r=0.35 * 0.336 / (0.5474 / 2.15), legs=True, trim=True),
    dict(id="truck", name="Truck", chassis="game/car-truck.png", size=(7.0137, 7.0137),
         wheel_reuse="monster", wheel_r=0.7, trim=True),
]

# Bill: his head is drawn on his head (a circle of 0.33 m) as wide as the game draws it (0.855 m: 57 pixels of its
# 0.96 m square picture of 64); his body from his hips (the bottom of his torso, a 0.6 m box) to his neck (its top):
# the body picture's hips and neck pixels; his legs on bikes (the leg picture, its hip, its height)
BILL = dict(head="Driver-head.png", head_m=0.855, body="Driver-body.png", body_neck=(34, 10), body_hips=(36, 110),
            torso_m=0.6, leg="Bike-driver-leg-full.png", leg_hip=(12, 28), leg_m=0.78)
