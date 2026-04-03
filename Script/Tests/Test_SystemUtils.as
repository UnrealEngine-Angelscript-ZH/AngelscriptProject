class FPhase2SystemUtilsFixture
{
	int Read()
	{
		return 13;
	}
}

int SystemUtilsFixtureValue()
{
	FPhase2SystemUtilsFixture Fixture;
	return Fixture.Read();
}
