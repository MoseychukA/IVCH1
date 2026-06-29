#include "NetFeed.h"
#include "SIM800TimeAsync.h"

volatile bool gNetUpdated = false;

bool gNetRegistered = false;
uint8_t gCregStat = 0;

int16_t gRssiDbm = 0;
uint8_t gSignalBars = 0;
int8_t gCsqRssi = 99;
uint8_t gCsqBer = 99;

char gIp1Str[16] = "0.0.0.0";
char gIp2Str[16] = "0.0.0.0";
volatile bool gIpUpdated = false;


bool NetFeed_UpdateFromSim(const SIM800TimeAsync& sim)
{
	bool changed = false;

	bool netReg = sim.networkRegistered();
	uint8_t creg = sim.cregStat();

	int16_t dbm = sim.rssiDbm();
	uint8_t bars = sim.signalBars();
	int8_t rssi = sim.csqRssi();
	uint8_t ber = sim.csqBer();

	if (gNetRegistered != netReg) { gNetRegistered = netReg; changed = true; }
	if (gCregStat != creg) { gCregStat = creg; changed = true; }

	if (gRssiDbm != dbm) { gRssiDbm = dbm; changed = true; }
	if (gSignalBars != bars) { gSignalBars = bars; changed = true; }
	if (gCsqRssi != rssi) { gCsqRssi = rssi; changed = true; }
	if (gCsqBer != ber) { gCsqBer = ber; changed = true; }

	if (changed) gNetUpdated = true;
	return changed;
}