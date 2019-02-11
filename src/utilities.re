/**
 * Substitute for Belt.Option.getExn
 * that raises custom exception if option contains no value
 */
let unwrap = (opt: option('a), exc: exn) => switch(opt){ 
  | Some(value) => value
  | None => raise(exc)
};

[@bs.val] [@bs.scope "JSON"]
external parse: string => Js.Json.t = "parse";

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
  external _getOwnPropertySymbols: 'a => array(Symbol.t) = "getOwnPropertySymbols";

  let getOwnPropertySymbols = obj => switch(Js.Types.classify(obj)){
  | JSObject(obj) => _getOwnPropertySymbols(obj)
  | _ => [||]
  };
};

module Serializer {
  module Exceptions {
    exception UnexpectedSerializedVariantKey(string);
    exception PolyVarTagNotPresentWhileExpected(string);
    exception UnexpectedActionType(string);
  };

  module DebugSymbol {
    [@bs.deriving jsConverter]
    type t = [
      | [@bs.as "Symbol(BsVariant)"] `BsVariant
      | [@bs.as "Symbol(BsPolyVar)"] `BsPolyVar
      | [@bs.as "Symbol(BsRecord)"] `BsRecord
      | [@bs.as "Symbol(ReductiveDevToolsBsLabeledVariant)"] `DevToolsBsLabeledVariant
    ];

    let ofReasonAction = action => {
      let symbols = Object.getOwnPropertySymbols(action);
      let extractedSymbols = symbols
        |> Array.mapi((idx, symbol) => (idx, {j|$symbol|j}));

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
    let viewAsDict: 'a => Js.Dict.t('b) = action => Obj.magic(action); 
    let viewAsArray: 'a => array('b) = action => Obj.magic(action);
    let viewAsList: 'a => list('b) = action => Obj.magic(action);
    let viewAsOpenObject: 'a => Js.t({..}) = action => Obj.magic(action);

    module Type {
      [@bs.deriving jsConverter]
      type t = [
        /* next three require -bs-g specified */
        | `VariantC
        | `PolyVarC
        | `Record
        /* variants without constructors and everything else belong here */
        | `Raw
      ];

      let fromReasonAction: 'a => t = action => 
        switch(Belt.Array.get(DebugSymbol.ofReasonAction(action), 0)){
        | Some((_, `BsVariant)) => `VariantC
        | Some((_, `BsPolyVar)) => `PolyVarC
        | Some((_, `BsRecord)) when Js.Array.isArray(action) => `Record
        | _ => `Raw
        }
    };

    /**
     * additional metadata that can be passed with variant actions 
     * to customize keys and action name
     */
    module UserMeta {
      [@bs.deriving abstract]
      type t = {
        [@bs.optional] keys: array(string),
        [@bs.optional] actionName: string
      };

      let fromReasonAction: 'a => option(t) = action => DebugSymbol.symbolValue(action, `DevToolsBsLabeledVariant)
    };

    module Internals {
      [@bs.deriving abstract]
      type t('a) = {
        kind: string,

        [@bs.optional] rawValue: 'a,
        [@bs.optional] polyVarTag: int,
        /* runtime representation of regular variants with more then one constructor contains tag */
        [@bs.optional] tag: int,
        [@bs.optional] userMeta: UserMeta.t,
        [@bs.optional] recordKeys: array(string),

        /* utilized on object serialization */
        [@bs.optional] isList: bool
      };

      let fromReasonAction = action => {
        let actionType = Type.fromReasonAction(action); 
        let userMeta = UserMeta.fromReasonAction(action);
        switch(actionType){
        | `Raw => 
          t(~kind=Type.tToJs(actionType),
            ~rawValue=action,
            ~userMeta?,
            ())
        | `Record => 
          t(~kind=Type.tToJs(actionType),
            ~recordKeys=?DebugSymbol.symbolValue(action, `BsRecord),
            ())
        | `VariantC => 
          t(~kind=Type.tToJs(actionType),
            ~tag=?Js.Dict.get(viewAsDict(action), "tag"),
            ~userMeta?,
            ())
        | `PolyVarC => 
          t(~kind=Type.tToJs(actionType),
            ~tag=?Js.Dict.get(viewAsDict(action), "tag"),
            ~polyVarTag=?Belt.Array.get(viewAsArray(action), 0),
            ~userMeta?,
            ())
        };
      }
    };

    type t('a, 'b) = Js.t({
      ..
      /*
       * denotes the action name displayed in the monitor 
       * for an actual action 'type', please check Internals.t._type
       */
      _type: string,
      __internal: Internals.t('b)
    }) as 'a;

    let _actionName = (action, internals) => {
      let userMetaName = internals
        |. Internals.userMetaGet
        |. Belt.Option.flatMap(userMeta => userMeta|.UserMeta.actionNameGet);
      let actionType = Type.fromReasonAction(action);
      switch((userMetaName, actionType)){
      | (Some(userMetaName), _) => userMetaName
      | (_, `VariantC) => Belt.Option.getWithDefault(DebugSymbol.symbolValue(action, `BsVariant), "update")
      | (_, `PolyVarC) => Belt.Option.getWithDefault(DebugSymbol.symbolValue(action, `BsPolyVar), "update")
      | _ => "update" 
      }
    };

    let rec serializeObject = (obj: 'a) => {
      let isList = DebugSymbol.symbolValue(obj, `BsVariant)
      |. Belt.Option.map(variantName => variantName == "::")
      let recordKeys = DebugSymbol.symbolValue(obj, `BsRecord)
      let base = [%bs.obj { 
        __internal: Internals.t(
          ~kind=Type.tToJs(`Raw),
          ~recordKeys?, 
          ~isList?,
          ())
      }];

      let serialized = 
        switch(Js.Types.classify(obj), Type.fromReasonAction(obj)){
        | (JSObject(obj), `Record) =>
          _serializeRecordToDict(~obj=Obj.magic(obj))
        | (JSObject(obj), _) when Js.Array.isArray(obj) => { 
          let array = Belt.Option.getWithDefault(isList, false) ? Array.of_list(viewAsList(obj)) : viewAsArray(obj);
          Array.map(entity => serializeObject(entity), array) |> viewAsDict
        }
        | (JSObject(obj), _) when !Js.Array.isArray(obj) => {
          let dict: Js.Dict.t('a) = viewAsDict(obj);
          Js.Dict.map((. entity) => serializeObject(entity), dict)
        }
        | _ => Obj.magic(obj)
        };

      switch(Js.Types.classify(serialized)){
      | JSObject(serialized) => Js.Obj.assign(base, viewAsOpenObject(serialized))
      | _ => viewAsOpenObject(serialized)
      };
    
    } and _serializeRecordToDict = (~obj: 'a) => {
      let keys = DebugSymbol.symbolValue(obj, `BsRecord);
      switch(keys){
      | None => 
        /* turns array into object with serialization applied to each value */
        Js.Obj.assign(
          Js.Obj.empty(),
          Array.map(entity => serializeObject(entity), viewAsArray(obj)) |> viewAsOpenObject
        ) |> viewAsDict
      | Some(keys) =>
        Belt.Array.reduceWithIndex(
          viewAsArray(obj),
          Js.Dict.empty(),
          (target, value, idx) => {
            let key = Belt.Array.get(keys, idx);
            Js.Dict.set(
              target,
              Belt.Option.getWithDefault(key, string_of_int(idx)),
              serializeObject(value)
            );
            target
          })
      }
    };

    let _serializeVariantToDict = (~action: 'a, ~isPolyVar: bool) => {
      let keys = DebugSymbol.symbolValue(action, `DevToolsBsLabeledVariant)
        |. Belt.Option.flatMap(meta => meta|.UserMeta.keysGet);
      switch(keys){
      | None => 
        Js.Obj.assign(
          Js.Obj.empty(), 
          Array.map(entity => serializeObject(entity), viewAsArray(action)) |> viewAsOpenObject
        )
      | Some(keys) => {
        Belt.Array.reduceWithIndex(
          isPolyVar ? viewAsArray(Obj.magic(action)[1]) : viewAsArray(action),
          Js.Dict.empty(),
          (target, value, idx) => {
            let variantKey = Belt.Array.get(keys, idx);
            Js.Dict.set(
              target,
              Belt.Option.getWithDefault(variantKey, string_of_int(idx)),
              serializeObject(value)
            );
            target
          }
        ) |> viewAsOpenObject
      }}
    };

    let rec deserializeObject = (~obj: Js.t({.. __internal: Js.Undefined.t(Internals.t('a))})) => {
      let isList = obj##__internal
        |. Js.Undefined.toOption
        |. Belt.Option.flatMap(Internals.isListGet)
        |. Belt.Option.getWithDefault(false);
      let recordKeys = obj##__internal
        |. Js.Undefined.toOption
        |. Belt.Option.flatMap(Internals.recordKeysGet);

      switch((Js.Types.classify(obj), recordKeys)){
      | (JSObject(_objectValue), Some(_recordKeys)) => _deserializeRecord(~obj) |> Obj.magic
      | (JSObject(objectValue), _) => {
        let deserialized = Js.Dict.keys(viewAsDict(objectValue))
          |. Belt.Array.keep(key => key != "__internal")
          |> Array.map(key => deserializeObject(~obj=Js.Dict.unsafeGet(viewAsDict(objectValue), key)));
        let deserialized = (isList ? viewAsArray(Array.to_list(deserialized)) : deserialized) |> Obj.magic;
        if(isList){
          Symbol.setValue(deserialized, Symbol.create("BsVariant"), "::");
        };
        
        deserialized
      }
      | _ => Obj.magic(obj)
      };
      
    } and _deserializeRecord = (~obj: Js.t({.. __internal: Js.Undefined.t(Internals.t('a))})) => {
      let recordKeys = obj##__internal
        |. Js.Undefined.toOption
        |. Belt.Option.flatMap(Internals.recordKeysGet);

      let deserialized = Js.Dict.keys(viewAsDict(obj))
        |. Belt.Array.keep(key => key != "__internal")
        |> Array.map(key => deserializeObject(~obj=Js.Dict.unsafeGet(viewAsDict(obj), key)));

      switch(recordKeys){
      | Some(recordKeys) => Symbol.setValue(deserialized, Symbol.create("BsRecord"), recordKeys) 
      | None => ()
      };

      deserialized
    };

    let _deserializeVariant = (~action: t('b, 'action), ~actionType: Type.t) => {
      let internal = action##__internal;
      let keys = internal
        |. Internals.userMetaGet
        |. Belt.Option.flatMap(userMeta => userMeta|.UserMeta.keysGet);
      let varTag = internal |.Internals.tagGet;
      let polyVarTag = internal
        |. Internals.polyVarTagGet;
      let deserialized = 
        switch(keys){
        | None => 
          Js.Dict.keys(viewAsDict(action))
            |. Belt.Array.keep(key => key != "__internal" && key != "type")
            |> Array.map(key => deserializeObject(~obj=Js.Dict.unsafeGet(viewAsDict(action), key)))
        | Some(keys) =>
          Js.Dict.keys(viewAsDict(action))
            |. Belt.Array.keep(key => key != "__internal" && key != "type")
            |> Array.fold_left((deserialized, key) => {
              let idx = Js.Array.findIndex(entity => entity == key, keys);
              if(idx == -1){
                raise(Exceptions.UnexpectedSerializedVariantKey({j|Serialized variant key($key) corresponding array idx has not been found.|j}))
              } else {
                deserialized[idx] = Js.Dict.unsafeGet(viewAsDict(action), key)
              };

              deserialized
            }, Belt.Array.makeUninitializedUnsafe(Array.length(keys)));
        };

      let actionTypeStr = Type.tToJs(actionType);
      switch(actionType){
      | `PolyVarC => {
        let polyVarTag = polyVarTag |. unwrap(Exceptions.PolyVarTagNotPresentWhileExpected("PolyVar tag not present while expected"));
        let deserialized = switch(keys){
        | None => {
          deserialized[0] = Obj.magic(polyVarTag);
          deserialized
        }
        | Some(_) => { 
          let deserialized = [|Obj.magic(polyVarTag), Obj.magic(deserialized)|];
          Symbol.setValue(deserialized, Symbol.create("ReductiveDevToolsBsLabeledVariant"), internal|.Internals.userMetaGet);
          deserialized
        }};

        Symbol.setValue(deserialized, Symbol.create("BsPolyVar"), action##_type);
        deserialized
      }
      | `VariantC => {
        Symbol.setValue(deserialized, Symbol.create("BsVariant"), action##_type);
        switch(varTag){
        | None => deserialized
        | Some(varTag) => {
          Js.Dict.set(viewAsDict(deserialized), "tag", varTag);
          deserialized
        }}
      }
      | _ => raise(Exceptions.UnexpectedActionType({j|$(actionTypeStr)|j}))
      }
    }

    let fromReasonAction: 'action => t('b, 'action) = action => {
      let internal = Internals.fromReasonAction(action);
      let actionName = _actionName(action, internal);
      let base = [%bs.obj {_type: actionName, __internal: internal}];

      switch(Type.fromReasonAction(action)){
      | `Raw => base
      | `Record => Js.Obj.assign(base, _serializeRecordToDict(~obj=action) |> viewAsOpenObject)
      | `VariantC => Js.Obj.assign(base, _serializeVariantToDict(~action, ~isPolyVar=false))
      | `PolyVarC => Js.Obj.assign(base, _serializeVariantToDict(~action, ~isPolyVar=true))
      }
    }

    let toReasonAction = (action: t('b, 'action)) => {
      let internal = action##__internal;
      let actionType = internal
        |. Internals.kindGet 
        |> Type.tFromJs
        |. Belt.Option.getWithDefault(`Raw);

      switch(actionType){
      | `Raw => internal|.Internals.rawValueGet|.Belt.Option.getExn
      | `Record => _deserializeRecord(~obj=Obj.magic(action)) |> Obj.magic
      | `VariantC
      | `PolyVarC => _deserializeVariant(~action, ~actionType) |> Obj.magic
      };
    }
  };

  let serializeAction = action => Action.fromReasonAction(action);
  let serializeObject = obj => Action.serializeObject(obj);

  let deserializeAction = action => Action.toReasonAction(action);
  let deserializeObject = obj => Action.deserializeObject(~obj);
};

let labelVariant: ('a, array(string)) => 'a = (variant, keys) => {
  Symbol.setValue(variant, Symbol.create("ReductiveDevToolsBsLabeledVariant"), Serializer.Action.UserMeta.t(
    ~keys,
    ()
  ));
  variant
};

let tagVariant: ('a, string) => 'a = (variant, name) => {
  Symbol.setValue(variant, Symbol.create("BsVariant"), name);
  variant
};

let tagList: list('a) => list('a) = list =>
  switch(Js.Types.classify(list)){
  | JSObject(_) => tagVariant(list, "::")
  | _ => list
  };

let tagPolyVar: ('a, string) => 'a = (polyVar, name) => {
  Symbol.setValue(polyVar, Symbol.create("BsPolyVar"), name);
  polyVar
};

let tagRecord: ('a, array(string)) => 'a = (obj, keys) => {
  Symbol.setValue(obj, Symbol.create("BsRecord"), keys);
  obj;
}