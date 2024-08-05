use std::fmt::{Binary, Debug, LowerHex};

pub struct HexNum<N: LowerHex>(pub N);

impl<N: LowerHex> Debug for HexNum<N> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        LowerHex::fmt(&self.0, f)
    }
}

pub struct BinNum<N: Binary>(pub N);

impl<N: Binary> Debug for BinNum<N> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Binary::fmt(&self.0, f)
    }
}
