#pragma once

class LawnApp;

// All game mutations run on the main update thread, outside window callbacks.
void UpdateDebugTrainer(LawnApp* app);
void OpenDebugTrainer();
int DebugTrainerSpeed();
void DestroyDebugTrainer();
