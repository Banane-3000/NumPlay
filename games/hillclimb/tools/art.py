"""Where everything sits: the vehicles' pictures over their bodies, and how big every picture is kept.

Vehicles are measured in the pixels of their chassis picture (x right, y down), as the wiki keeps it; meters come
from the distance between their first and last axles (`wheelbase`). tools/pack.py turns this into src/art.h, and
tools/preview.py draws each vehicle put together, to check the numbers."""

# Each vehicle: its chassis picture; its axles (the wheels' centres at rest, in the chassis' pixels); the wheel's
# picture, the radius of its tyre in that picture and in meters (one per axle when they differ); the distance
# between the first and last axles in meters; where Bill's hips go (chassis pixels), how he leans (degrees, back)
VEHICLES = [
    dict(id="jeep", name="Hill Climber", chassis="Chassis-hillclimber-red.png",
         axles=[(50, 108), (205, 108)], wheelbase=2.15, wheel="Common_Wheel_-_Type_1.png", wheel_px=31,
         wheel_m=[0.43, 0.43], driver=dict(hips=(120, 62), lean=4)),
    dict(id="bike", name="Motocross Bike", chassis="Bike-motocross.png",
         axles=[(42, 126), (218, 126)], wheelbase=1.85, wheel="Bike-motocross-tire.png", wheel_px=31,
         wheel_m=[0.44, 0.44], driver=dict(hips=(104, 40), lean=-16, legs=True),
         # the swingarm (engine -> rear axle) and fork (handlebar -> front axle), drawn as tubes
         tubes=[((128, 104), 0, 0x5A5E66, 0.09), ((176, 30), 1, 0x30343A, 0.07)]),
    dict(id="monster", name="Monster Truck", chassis="Car-monster.png",
         axles=[(50, 99), (210, 99)], wheelbase=2.55, wheel="Car-monster-tire.png", wheel_px=31,
         wheel_m=[0.62, 0.62], driver=dict(hips=(120, 56), lean=4)),
    dict(id="quad", name="Quad Bike", chassis="Car-quadbike.png",
         axles=[(54, 106), (204, 106)], wheelbase=1.75, wheel="Common_Wheel_-_Type_4.png", wheel_px=62,
         wheel_m=[0.5, 0.5], driver=dict(hips=(116, 32), lean=-12, legs=True)),
    dict(id="race", name="Race Car", chassis="Car-racecar.png",
         axles=[(62, 86), (204, 86)], wheelbase=2.45, wheel="Common_Wheel_-_Type_2.png", wheel_px=31,
         wheel_m=[0.4, 0.4], driver=dict(hips=(126, 68), lean=22)),
    dict(id="rally", name="Rally Car", chassis="Car-rallycar.png",
         axles=[(53, 92), (211, 92)], wheelbase=2.35, wheel="Common_Wheel_-_Type_8.png", wheel_px=31,
         wheel_m=[0.42, 0.42], driver=dict(hips=(130, 62), lean=6)),
    dict(id="buggy", name="Dune Buggy", chassis="Dune_Buggy_Chassis_(Sprite).png",
         axles=[(48, 88), (206, 84)], wheelbase=2.3, wheel="buggy-tire", wheel_px=39,
         wheel_m=[0.52, 0.4], driver=dict(hips=(86, 66), lean=10)),
    dict(id="tank", name="Tank", chassis="Car-tank-fg.png",
         axles=[(40, 96), (84, 102), (128, 102), (172, 102), (216, 96)], wheelbase=3.2, wheel="Car-tank-tire.png",
         wheel_px=31, wheel_m=[0.3] * 5, driver=dict(hips=(130, 44), lean=0),
         # the tracks' base under the hull and the cannon over it: (picture, its pixel on the chassis' (0, 0),
         # its scale to the chassis, in front)
         layers=[("Car-tank-bg.png", (14, 18), 0.92, False), ("Car-tank-cannon.png", (140, -26), 0.42, True)]),
]

# Bill: his head and body pictures and where they join (picture pixels): the base of the neck on the body, the
# neck under the chin on the head, his hips at the bottom of the body; how big they are (meters, the pictures'
# heights); his legs on bikes (the leg picture, its hip and its size)
BILL = dict(head="Driver-head.png", body="Driver-body.png", head_neck=(52, 104), body_neck=(34, 10),
            body_hips=(36, 110), body_m=0.98, head_m=0.84, leg="Bike-driver-leg-full.png", leg_hip=(12, 28),
            leg_m=0.78)
