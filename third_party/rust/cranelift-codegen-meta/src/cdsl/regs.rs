use cranelift_entity::EntityRef;

#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct RegBankIndex(u32);
entity_impl!(RegBankIndex);

pub struct RegBank {
    pub name: &'static str,
    pub first_unit: u8,
    pub units: u8,
    pub names: Vec<&'static str>,
    pub prefix: &'static str,
    pub pressure_tracking: bool,
    pub toprcs: Vec<RegClassIndex>,
    pub classes: Vec<RegClassIndex>,
}

impl RegBank {
    pub fn new(
        name: &'static str,
        first_unit: u8,
        units: u8,
        names: Vec<&'static str>,
        prefix: &'static str,
        pressure_tracking: bool,
    ) -> Self {
        RegBank {
            name,
            first_unit,
            units,
            names,
            prefix,
            pressure_tracking,
            toprcs: Vec::new(),
            classes: Vec::new(),
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct RegClassIndex(u32);
entity_impl!(RegClassIndex);

pub struct RegClass {
    pub name: &'static str,
    pub index: RegClassIndex,
    pub width: u8,
    pub bank: RegBankIndex,
    pub toprc: RegClassIndex,
    pub count: u8,
    pub start: u8,
    pub subclasses: Vec<RegClassIndex>,
}

impl RegClass {
    pub fn new(
        name: &'static str,
        index: RegClassIndex,
        width: u8,
        bank: RegBankIndex,
        toprc: RegClassIndex,
        count: u8,
        start: u8,
    ) -> Self {
        Self {
            name,
            index,
            width,
            bank,
            toprc,
            count,
            start,
            subclasses: Vec::new(),
        }
    }

    /// Compute a bit-mask of subclasses, including self.
    pub fn subclass_mask(&self) -> u64 {
        let mut m = 1 << self.index.index();
        for rc in self.subclasses.iter() {
            m |= 1 << rc.index();
        }
        m
    }

    /// Compute a bit-mask of the register units allocated by this register class.
    pub fn mask(&self, bank_first_unit: u8) -> Vec<u32> {
        let mut u = (self.start + bank_first_unit) as usize;
        let mut out_mask = vec![0, 0, 0];
        for _ in 0..self.count {
            out_mask[u / 32] |= 1 << (u % 32);
            u += self.width as usize;
        }
        out_mask
    }
}

pub enum RegClassProto {
    TopLevel(RegBankIndex),
    SubClass(RegClassIndex),
}

pub struct RegClassBuilder {
    pub name: &'static str,
    pub width: u8,
    pub count: u8,
    pub start: u8,
    pub proto: RegClassProto,
}

impl RegClassBuilder {
    pub fn new_toplevel(name: &'static str, bank: RegBankIndex) -> Self {
        Self {
            name,
            width: 1,
            count: 0,
            start: 0,
            proto: RegClassProto::TopLevel(bank),
        }
    }
    pub fn subclass_of(
        name: &'static str,
        parent_index: RegClassIndex,
        start: u8,
        stop: u8,
    ) -> Self {
        assert!(stop >= start);
        Self {
            name,
            width: 0,
            count: stop - start,
            start: start,
            proto: RegClassProto::SubClass(parent_index),
        }
    }
    pub fn count(mut self, count: u8) -> Self {
        self.count = count;
        self
    }
    pub fn width(mut self, width: u8) -> Self {
        match self.proto {
            RegClassProto::TopLevel(_) => self.width = width,
            RegClassProto::SubClass(_) => panic!("Subclasses inherit their parent's width."),
        }
        self
    }
}

pub struct RegBankBuilder {
    pub name: &'static str,
    pub units: u8,
    pub names: Vec<&'static str>,
    pub prefix: &'static str,
    pub pressure_tracking: Option<bool>,
}

impl RegBankBuilder {
    pub fn new(name: &'static str, prefix: &'static str) -> Self {
        Self {
            name,
            units: 0,
            names: vec![],
            prefix,
            pressure_tracking: None,
        }
    }
    pub fn units(mut self, units: u8) -> Self {
        self.units = units;
        self
    }
    pub fn names(mut self, names: Vec<&'static str>) -> Self {
        self.names = names;
        self
    }
    pub fn track_pressure(mut self, track: bool) -> Self {
        self.pressure_tracking = Some(track);
        self
    }
}
