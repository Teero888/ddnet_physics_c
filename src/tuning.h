/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more
 * information. */
/* If you are missing that file, acquire a complete release at teeworlds.com. */

// This file can be included several times.

#define SERVER_TICK_SPEED 50

#ifndef MACRO_TUNING_PARAM
#define MACRO_TUNING_PARAM(Name, Value) ;
#endif

// physics tuning
MACRO_TUNING_PARAM(GroundControlSpeed, 10.0f)
MACRO_TUNING_PARAM(GroundControlAccel, 100.0f / SERVER_TICK_SPEED)
MACRO_TUNING_PARAM(GroundFriction, 0.5f)
MACRO_TUNING_PARAM(GroundJumpImpulse, 13.2f)
MACRO_TUNING_PARAM(AirJumpImpulse, 12.0f)
MACRO_TUNING_PARAM(AirControlSpeed, 250.0f / SERVER_TICK_SPEED)
MACRO_TUNING_PARAM(AirControlAccel, 1.5f)
MACRO_TUNING_PARAM(AirFriction, 0.95f)
MACRO_TUNING_PARAM(HookLength, 380.0f)
MACRO_TUNING_PARAM(HookFireSpeed, 80.0f)
MACRO_TUNING_PARAM(HookDragAccel, 3.0f)
MACRO_TUNING_PARAM(HookDragSpeed, 15.0f)
MACRO_TUNING_PARAM(Gravity, 0.5f)
MACRO_TUNING_PARAM(VelrampStart, 550)
MACRO_TUNING_PARAM(VelrampRange, 2000)
MACRO_TUNING_PARAM(VelrampCurvature,
                   0.33647223662121284f /* Precomputed logf(1.4)*/)
MACRO_TUNING_PARAM(GunCurvature, 1.25f)
MACRO_TUNING_PARAM(GunSpeed, 2200.0f)
MACRO_TUNING_PARAM(GunLifetime, 2.0f)
MACRO_TUNING_PARAM(ShotgunCurvature, 1.25f)
MACRO_TUNING_PARAM(ShotgunSpeed, 2750.0f)
MACRO_TUNING_PARAM(ShotgunSpeeddiff, 0.8f)
MACRO_TUNING_PARAM(ShotgunLifetime, 0.20f)
MACRO_TUNING_PARAM(GrenadeCurvature, 7.0f)
MACRO_TUNING_PARAM(GrenadeSpeed, 1000.0f)
MACRO_TUNING_PARAM(GrenadeLifetime, 2.0f)
MACRO_TUNING_PARAM(LaserReach, 800.0f)
MACRO_TUNING_PARAM(LaserBounceDelay, 150)
MACRO_TUNING_PARAM(LaserBounceNum, 1000)
MACRO_TUNING_PARAM(LaserBounceCost, 0)
MACRO_TUNING_PARAM(LaserDamage, 5)
MACRO_TUNING_PARAM(PlayerCollision, 1)
MACRO_TUNING_PARAM(PlayerHooking, 1)
MACRO_TUNING_PARAM(JetpackStrength, 400.0f)
MACRO_TUNING_PARAM(ShotgunStrength, 10.0f)
MACRO_TUNING_PARAM(ExplosionStrength, 6.0f)
MACRO_TUNING_PARAM(HammerStrength, 1.0f)
MACRO_TUNING_PARAM(HookDuration, 1.25f)
MACRO_TUNING_PARAM(HammerFireDelay, 125)
MACRO_TUNING_PARAM(GunFireDelay, 125)
MACRO_TUNING_PARAM(ShotgunFireDelay, 500)
MACRO_TUNING_PARAM(GrenadeFireDelay, 500)
MACRO_TUNING_PARAM(LaserFireDelay, 800)
MACRO_TUNING_PARAM(NinjaFireDelay, 800)
MACRO_TUNING_PARAM(HammerHitFireDelay, 320)
MACRO_TUNING_PARAM(GroundElasticityX, 0)
MACRO_TUNING_PARAM(GroundElasticityY, 0)
