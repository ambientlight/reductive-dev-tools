module Symbol {
  type t = Js.t({.});

  let getValue: ('b, t) => 'a;
  let setValue: ('a, t, 'b) => unit;
  let toString: t => string;
};

module Serializer {
  module DebugSymbol {
    type t = [
      // legacy pre-bs8 symbols
      // | [@bs.as "Symbol(BsVariant)"] `BsVariant
      // | [@bs.as "Symbol(BsPolyVar)"] `BsPolyVar
      // | [@bs.as "Symbol(BsRecord)"] `BsRecord
      // | [@bs.as "Symbol(ReductiveDevToolsBsLabeledVariant)"] `DevToolsBsLabeledVariant
      | [@bs.as "Symbol(name)"] `Name
    ];

    let ofReasonAction: 'a => array((int, t));
    let symbolValue: ('b, t) => option('a)
  }

  module Action {
    
    type t('a) = Js.t({
      ..
      /*
       * denotes the action name displayed in the monitor 
       * for an actual action 'type', please check Internals.t._type
       */
      _type: string
    }) as 'a;
  };

  let serializeAction: 'a => Action.t({. "_type": string});
  let deserializeAction: Action.t({. "_type": string}) => 'a;
};