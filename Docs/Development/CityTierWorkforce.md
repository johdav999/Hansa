# City and tier workforce pooling

Workforce uses the authored WorkforcePerResidentBasisPoints for each population
tier. Fractions are pooled within each city and tier, then rounded down once.
At 60%, four residents provide two workers regardless of their housing distribution.
Different cities and tiers never share fractional workers. Unfinished or otherwise
non-operational residences do not contribute.

The implementation carries the fractional remainder through canonical cohort ID
order, attributing whole workers back to residences. These integer contributions
sum exactly to the pooled total used by production, city queries, inspectors, and
diagnostic logs. A residence's contribution is therefore its allocation from the
city/tier pool, not an independent rounded workforce calculation.

Supply is recalculated before production staffing and again after migration.
Production continues to receive changed population on its next allocation tick.
The existing authored workforce rate, editor field, and save fields are unchanged;
loaded games use pooled calculation on their next simulation tick.

Regression coverage: Hansa.Simulation.Population.CityTierPooledWorkforce checks
split versus combined housing at both laborer and artisan rates, separate tier
pools, allocation to production, and immediate post-growth workforce totals.
