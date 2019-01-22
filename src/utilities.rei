module Serializer {
    [@bs.deriving jsConverter]
    type debugSymbol = [
        | [@bs.as "Symbol(BsVariant)"] `BsVariant
        | [@bs.as "Symbol(BsPolyVar)"] `BsPolyVar
        | [@bs.as "Symbol(BsRecord)"] `BsRecord
        | [@bs.as "Symbol(ReductiveDevToolsBsList)"] `DevToolsBsList
        | [@bs.as "Symbol(ReductiveDevTollsBsLabeledVariant"] `DevToolsBsLabeledVariant
    ];

    type serializedAction('a) = Js.t({
        ..
        _type: string
    }) as 'a;

    let serializeAction: Js.Types.obj_val => serializedAction('b);
};

let tagList: list('a) => list('a);
let tagVariant: ('a, ~keys: array(string)=?, ~actionName: string=?, unit) => 'a; 