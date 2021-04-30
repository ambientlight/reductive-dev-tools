module Symbol {
  type t = Js.t({.});

  [@bs.val][@bs.scope "Object"]
  external _defineSymbol: ('a, t, 'b) => unit = "defineProperty"; 

  let create: string => t = [%raw {|
    function(key){ return Symbol(key) }
  |}];

  let getValue = (obj, symbol: t) => Js.Dict.unsafeGet(Obj.magic(obj), Obj.magic(symbol));
  let setValue = (obj, symbol, value) => _defineSymbol(obj, symbol, [%bs.obj {
    value: value,
    writable: false
  }]);

  [@bs.send]
  external toString: t => string = "toString";
};

module Object {
  [@bs.val][@bs.scope "Object"]
  external _getOwnPropertySymbols: 'a => array(Symbol.t) = "getOwnPropertySymbols";

  let getOwnPropertySymbols = obj => switch(Js.Types.classify(obj)){
  | JSObject(obj) => _getOwnPropertySymbols(obj)
  | _ => [||]
  };
};

module Serializer {
  module DebugSymbol {
    [@bs.deriving jsConverter]
    type t = [
      // legacy pre-bs8 symbols
      // | [@bs.as "Symbol(BsVariant)"] `BsVariant
      // | [@bs.as "Symbol(BsPolyVar)"] `BsPolyVar
      // | [@bs.as "Symbol(BsRecord)"] `BsRecord
      // | [@bs.as "Symbol(ReductiveDevToolsBsLabeledVariant)"] `DevToolsBsLabeledVariant
      | [@bs.as "Symbol(name)"] `Name
    ];

    let ofReasonAction = action => {
      let symbols = Object.getOwnPropertySymbols(action);
      let extractedSymbols = symbols
        |> Array.mapi((idx, symbol) => (idx, symbol |. Symbol.toString));

      /** 
       * make sure Bs* symbols always appearing before ReductiveDevTool ones 
       * (logic will pick first symbol available)
       * assumption: Bs* symbols are exclusive, but they can combine with ReductiveDevTool* ones
       * which provide additional metadata(user specified)
       */
      Array.sort(((_, lhs), (_, rhs)) => compare(lhs, rhs), extractedSymbols);
      extractedSymbols
        |> Array.map(((idx, symbol)) => (idx, tFromJs(symbol)))
        |. Belt.Array.keep(((_, symbol)) => Belt.Option.isSome(symbol))
        |> Array.map( ((idx, symbol)) => (idx, Belt.Option.getExn(symbol)) );
      }

    let symbolValue = (action, debugSymbol: t) => 
      ofReasonAction(action)
      |. Belt.Array.keep(((_, symbol)) => symbol == debugSymbol)
      |. Belt.Array.get(0)
      |. Belt.Option.map(((idx, _)) => {
        let symbol = Array.get(Object.getOwnPropertySymbols(action), idx);
        Symbol.getValue(action, symbol)
      })
  };

  module Action {
   
    type t('a) = Js.t({
      ..
      /*
       * denotes the action name displayed in the monitor 
       * for an actual action 'type', please check Internals.t._type
       */
      _type: string
    }) as 'a;

    
    let fromReasonAction: 'action => t({. "_type": string}) = action => {
      DebugSymbol.symbolValue(action, `Name)
      |. Belt.Option.map(actionName => {
        let base = [%bs.obj {_type: actionName }];
        Js.Obj.assign(base, Obj.magic(action))
      })
      |. Belt.Option.getWithDefault(Obj.magic(action))
    }

    let toReasonAction = (action: t('s)) => {
      // not errasing a type field here,, if needed, use a customSerializer
      Obj.magic(action)
    }
  };

  let serializeAction = action => Action.fromReasonAction(action);
  let deserializeAction = action => Action.toReasonAction(action);
};