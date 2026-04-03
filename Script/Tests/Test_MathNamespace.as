class FPhase2MathFixture
{
	int Value = 10;
}

int MathNamespaceFixtureValue()
{
	FPhase2MathFixture Fixture;
	return Fixture.Value;
}
