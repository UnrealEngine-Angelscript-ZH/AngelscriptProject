class FPhase2ActorLifecycleProbe
{
	int Step()
	{
		return 21;
	}
}

int ActorLifecycleFixtureValue()
{
	FPhase2ActorLifecycleProbe Probe;
	return Probe.Step();
}
