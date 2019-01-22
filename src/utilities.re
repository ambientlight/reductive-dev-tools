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
};

module Object {
  [@bs.val][@bs.scope "Object"]
  external getOwnPropertySymbols: 'a => array(Symbol.t) = "getOwnPropertySymbols";
};

module Serializer {
  [@bs.deriving jsConverter]
  type debugSymbol = [
    | [@bs.as "Symbol(BsVariant)"] `BsVariant
    | [@bs.as "Symbol(BsPolyVar)"] `BsPolyVar
    | [@bs.as "Symbol(BsRecord)"] `BsRecord
    | [@bs.as "Symbol(ReductiveDevToolsBsList)"] `DevToolsBsList
    | [@bs.as "Symbol(ReductiveDevToolsBsLabeledVariant)"] `DevToolsBsLabeledVariant
  ];

  /**
   * additional metadata that can be passed with variant actions 
   * to custom keys and action name
   */
  [@bs.deriving abstract]
  type actionVariantMeta = {
    [@bs.optional] keys: array(string),
    [@bs.optional] actionName: string
  };

  type serializedAction('a) = Js.t({
    ..
    _type: string
  }) as 'a;

  let _processSymbols = (symbols: array(Symbol.t)) => {
    let extractedSymbols = symbols
      |> Array.mapi((idx, symbol) => (idx, {j|$symbol|j}));

    /** 
     * make sure Bs* symbols always appearing before ReductiveDevTool ones
     * assumption: Bs* symbols are exclusive, but they can combine with ReductiveDevTool* ones
     * which provide additional metadata(user specified)
     * 
     * use a first bs symbol and additional metadata 
     * from ReductiveDevTool originated symbols when needed
     */
    Array.sort(((_, lhs), (_, rhs)) => compare(lhs, rhs), extractedSymbols);
    extractedSymbols
      |> Array.map(((idx, symbol)) => (idx, debugSymbolFromJs(symbol)))
      |. Belt.Array.keep(((_, symbol)) => Belt.Option.isSome(symbol))
      |> Array.map( ((idx, symbol)) => (idx, Belt.Option.getExn(symbol)) );
  }

  let rec _serializeRecord = (~obj: 'a, ~symbol: Symbol.t, ~baseObject: Js.t({..})) => {
    let keys: array(string) = Symbol.getValue(obj, symbol);
    let serialized = Belt.Array.reduceWithIndex(
      Js.Obj.keys(Obj.magic(obj)),
      Js.Obj.assign(Js.Obj.empty(), baseObject),
      (target, key, idx) => {
        let value = Js.Dict.unsafeGet(Obj.magic(obj), key);
        let namedKey = Belt.Array.get(keys, idx);

        Js.Dict.set(
          Obj.magic(target),
          Belt.Option.isSome(namedKey) ? Belt.Option.getExn(namedKey) : key,
          serializeObject(value)
        );
        target
      }
    );

    Symbol.setValue(serialized, symbol, keys);
    serialized
  } and serializeObject = (obj: 'a) => {
    let symbols = Object.getOwnPropertySymbols(obj);
    let extractedSymbols = _processSymbols(symbols);
    let firstSymbol = Belt.Array.get(extractedSymbols, 0);

    let shouldParseListToArray = extractedSymbols 
      |. Belt.Array.keep(((_, symbol)) => symbol == `DevToolsBsList)
      |. Belt.Array.get(0)
      |. Belt.Option.map(((idx, _)) => Symbol.getValue(obj, Array.get(symbols, idx)))
      |. Belt.Option.getWithDefault(false);

    switch(Js.Types.classify(obj), firstSymbol){
    | (JSObject(obj), Some((idx, `BsRecord))) when Js.Array.isArray(obj) =>
      _serializeRecord(~obj, ~symbol=Array.get(symbols, idx), ~baseObject=Js.Obj.empty()) |> Obj.magic
    | (JSObject(obj), _) when Js.Array.isArray(obj) => { 
      let array = shouldParseListToArray ? Array.of_list(Obj.magic(obj)) : Obj.magic(obj);
      Array.map(entity => serializeObject(entity), array) |> Obj.magic
    }
    | (JSObject(obj), _) when !Js.Array.isArray(obj) => {
      let dict: Js.Dict.t('a) = Obj.magic(obj);
      Js.Dict.map((. entity) => serializeObject(entity), dict) |> Obj.magic
    }
    | _ => Obj.magic(obj)
    };
  } 
  
  let _serializeUserTaggedAction = (~obj, ~symbol: Symbol.t, ~variantName: string, ~isPolyVar: bool) => {
    let variantUserMeta: actionVariantMeta = Symbol.getValue(obj, symbol);
    let variantKeys = variantUserMeta|.keysGet|.Belt.Option.getWithDefault([||]);
    let actionType = variantUserMeta
      |.actionNameGet
      |.Belt.Option.getWithDefault(variantName);

    let serialized = Belt.Array.reduceWithIndex(
      /* when polyvar, 0 idx corresponds to its global tag, filter out here and reassign later */
      Js.Obj.keys(Obj.magic(obj)) |. Belt.Array.keep(key => isPolyVar ? key != "0" : true),
      Js.Obj.assign(Js.Obj.empty(), [%bs.obj {_type: actionType}]),
      (target, key, idx) => {
        let value = Js.Dict.unsafeGet(Obj.magic(obj), key);
        let variantKey = Belt.Array.get(variantKeys, idx);

        Js.Dict.set(
          Obj.magic(target),
          Belt.Option.isSome(variantKey) ? Belt.Option.getExn(variantKey) : key,
          serializeObject(value)
        );
        target
      }
    );

    let serialized = isPolyVar 
      ? Js.Obj.assign(serialized, [%bs.obj {__polyVarTag: Js.Dict.unsafeGet(Obj.magic(obj), "0")}]) 
      : serialized;
   
    /* we need to keep the ReductiveDevToolsBsLabeledVariant symbol, so we later can deserialize it back */
    Symbol.setValue(serialized, symbol, variantUserMeta);
    serialized
  };

  let serializeAction = (obj: 'a): serializedAction('b) => {
    let symbols = Object.getOwnPropertySymbols(obj);
    let extractedSymbols = _processSymbols(symbols);
    
    /**
     * additional user-provided metadata 
     */
    let additionalMetaSymbol = extractedSymbols 
      |. Belt.Array.keep(((_, symbol)) => symbol == `DevToolsBsLabeledVariant)
      |. Belt.Array.get(0);
    /* if ReductiveDevToolsBsLabeledVariant is provided action type can be customized */
    let baseActionType = additionalMetaSymbol 
      |. Belt.Option.flatMap(((idx, _)) => { 
        let metaSymbol = Array.get(symbols, idx);
        let userMeta: actionVariantMeta = Symbol.getValue(obj, metaSymbol);
        userMeta|.actionNameGet})
      |. Belt.Option.getWithDefault("update");
    
    let firstSymbol = Belt.Array.get(extractedSymbols, 0);
    switch(firstSymbol){
    /*
     * -bs-g flag is used, variants with constructors 
     * if ReductiveDevToolsBsLabeledVariant is provided both action type and keys can be customized 
     */
    | Some((idx, `BsVariant as debugSymbol))
    | Some((idx, `BsPolyVar as debugSymbol)) when Js.Array.isArray(obj) => {
      let symbol = Array.get(symbols, idx);
      let variantName: string = Symbol.getValue(obj, symbol);

      let serialized = 
        switch(additionalMetaSymbol){
        | Some((metaSymbolIdx, _)) => { 
          let metaSymbol = Array.get(symbols, metaSymbolIdx);
          _serializeUserTaggedAction(~obj, ~symbol=metaSymbol, ~variantName, ~isPolyVar=debugSymbol==`BsPolyVar)
        }
        | None =>
          Js.Obj.assign(
            Js.Obj.assign(Js.Obj.empty(), Obj.magic(Array.map(entity => serializeObject(entity), obj |> Obj.magic))), 
            [%bs.obj {_type: variantName}]
          );
        };

      Symbol.setValue(serialized, symbol, variantName);
      serialized
    }
    /* -bs-g flag is used, and for some reason action is record */
    | Some((idx, `BsRecord)) when Js.Array.isArray(obj) =>
      _serializeRecord(~obj, ~symbol=Array.get(symbols, idx), ~baseObject=[%bs.obj {_type: baseActionType}])
    
    /* anything else gets wrapped and carried inside __rawValue */
    | _ => Js.Obj.assign(Js.Obj.empty(), [%bs.obj {_type: baseActionType, __rawValue: serializeObject(obj)}])
    };
  };
};

let tagList: list('a) => list('a) = list => {
  Symbol.setValue(list, Symbol.create("ReductiveDevToolsBsList"), true);
  list
}

let tagVariant: ('a, ~keys: array(string)=?, ~actionName: string=?, unit) => 'a = (variant, ~keys=?, ~actionName=?, ()) => {
  Symbol.setValue(variant, Symbol.create("ReductiveDevToolsBsLabeledVariant"), Serializer.actionVariantMeta(
    ~keys?,
    ~actionName?,
    ()
  ));
  variant
}