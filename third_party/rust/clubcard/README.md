# Clubcard

Clubcard is an exact membership query filter for static sets.

It is based on the *Ribbon filters* of Dillinger and Walzer[^1] and Dillinger, Hübschle-Schneider, Sanders, and Walzer[^2]. And it makes use of a partitioning strategy described by Mike Hamburg in his Real World Crypto 2022 talk[^3].

> [!WARNING]
> This is work in progress. Neither the API nor the serialization format for clubcard are stable.

## Application to CRLite and performance

CRLite[^4] publishes the revocation status of certificates in the WebPKI in a compact exact membership query filter. The system, as described in[^4] and as implemented by Mozilla, encodes revocation statuses in a *Bloom filter cascade*.

The Bloom filter cascade published by Mozilla's CRLite infrastructure on 2024-08-29 is 19.3 MB. That filter encodes the revocation status of 816 million certificates from 795 distinct issuers. Out of the 816 million certificates, 11.7 million are revoked.

Clubcard was developed as a replacement for the Bloom filter cascade in CRLite. A clubcard for the 2024-08-29 revocation status dataset is **8.5 MB**&mdash;a 56% reduction in size.

Some of the improvement is due to Clubcard partitioning the set of revocations by issuer. The information theoretic lower bound for encoding an generic 11.7 million element subset of an 816 million element set is 11 MB; whereas the lower bound for encoding the 795 sets obtained by partitioning the revocations by issuer is 7.6 MB. CRLite does not employ the partitioning-by-issuer trick, but we estimate that Bloom filter cascades for the 2024-08-29 data set sharded by issuer would be ~13MB.

The remainder of the improvement is due to the use of Ribbon filters. 

While we have not finished implementing all of the tricks employed by state-of-the-art Ribbon filter implementations, our 8.5 MB clubcard filter for is within 12% of the 7.6 MB lower bound for the data set. We believe the overhead can be reduced to ~8% without any new ideas.

> [!NOTE]
> The "information theoretic lower bound" should be taken with a grain of salt, as it depends on how the set of certificates is partitioned. There are many other ways to partition the set of certificates, e.g. by issuer AND the month of the notAfter date. We are currently exploring whether other partitioning strategies could further reduce filter size.

## Mathematical intuition

We say that a set U is *filterable* if, for every positive integer m, there is a hash function h<sub>m</sub> : U →  ({0,...,m-1}, {0,1}<sup>256</sup>). We view tuples (s, a) in the range of h<sub>m</sub> as elements of (**F**<sub>2</sub>)<sup>d</sup>, for any d, by equating (s, a) with \[0<sup>s</sup> | a | 0<sup>d - s - 256</sup>\] (or some prefix thereof if d < s + 256).

Let R be a subset of a filterable set U. A *clubcard for R* is a pair of matrices X and Y defined over **F**<sub>2</sub> and a pair of hash functions h and g. The matrices and hash functions are chosen such that
R = { u ∈ U : h(u) · X = 0 ∧ g(u) · Y = 0 }.

Given a clubcard for R and an element u of U, one can determine membership of u in R by computing h(u) · X and g(u) · Y.

To produce a clubcard for R ⊆ U, we:
1) Enumerate the elements of R (arbitrarily, any order will do).
2) Let m = (1+epsilon)|R|.
3) Let H be the |R| × m matrix with rows given by h<sub>m</sub>(r) for r ∈ R.
4) Let X be a solution to H · X = 0<sup>|R| x k</sup> where k = floor(lg(|U \ R| / |R|)).
5) Let S = {u ∈ U : h(u) · X = 0}.
6) Let m' = (1+epsilon)|S|.
7) Let G be the |S| × m' matrix with rows given by h<sub>m'</sub>(s) for s ∈ S
8) Let C be the |S| × 1 matrix with the row for element s equal to 0 iff s ∈ R.
9) Let Y be a solution to G · Y = C.

The clubcard is (X, Y, m, m').

We use the fast algorithm for solving the systems in steps 4 and 9 from [^1][^2]. The system in step 4 is always solvable, but the epsilon parameter must be chosen carefully to ensure that |S| is not too large. The system in step 9 may not be solvable. So, in addition to (X, Y, m, m'), clubcards may be published with a list of elements of U\R that are not encoded correctly. The epsilon parameter can be chosen to ensure that this list is small with high probability. Alternatively one may repeat the construction until the system is solvable, e.g. by tweaking the definition of U to include a seed which is used to randomize the hash functions.

## When partitioning is a good idea

The binomial coefficient n choose r is approximately equal to 2<sup>n h(r/n)</sup> where h is the binary entropy function (this follows from Stirling's approximation for n!).
The number of bits required to encode an arbitrary r element subset R of an n element set U is therefore  Ω(lg(n choose r)) = Ω(n h(r/n)). The binary entropy function is concave, so

&nbsp;&nbsp;&nbsp;&nbsp;n<sub>1</sub> h(r<sub>1</sub>/n<sub>1</sub>) + n<sub>2</sub> h(r<sub>2</sub>/n<sub>2</sub>) ≤ (n<sub>1</sub> + n<sub>2</sub>) h((r<sub>1</sub>+r<sub>2</sub>)/(n<sub>1</sub>+n<sub>2</sub>)).

It follows that if {U1, U2} is a partition of U, then one may be able to encode the pair (R1, R2) in fewer bits than it would take to encode R.

> [!NOTE]
> For partitioning to be beneficial, the values |Ri|/|Ui| must vary considerably between blocks of the partition.

## Minor optimizations

TODO:
- How this library handles partitions.
- Benefits of sorting partitions by decreasing rank(Xi) and storing only the coefficients x<sub>i,j</sub> of X where j < rank(Xi)
- Cache locality of queries / use of interleaved column-major order from[^2].
- Compact encoding of R = {} and R = U cases.
- .....



[^1]: Peter C. Dillinger, Stefan Walzer. "Ribbon filter: practically smaller than Bloom and Xor". https://arxiv.org/pdf/2103.02515
[^2]: Peter C. Dillinger, Lorenz Hübschle-Schneider, Peter Sanders, Stefan Walzer. "Fast Succinct Retrieval and Approximate
Membership using Ribbon". https://arxiv.org/pdf/2109.01892
[^3]: Mike Hamburg. "Improved CRL compression with structured linear functions". https://youtu.be/Htms5rNy7B8?list=PLeeS-3Ml-rpovBDh6do693We_CP3KTnHU&t=2357
[^4]: James Larisch, David Choffnes, Dave Levin, Bruce M. Maggs, Alan Mislove, Christo Wilson. "CRLite: A Scalable System for Pushing All TLS Revocations to All Browsers". https://jameslarisch.com/pdf/crlite.pdf
