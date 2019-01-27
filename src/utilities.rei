let unwrap: (option('a), exn) => 'a;
let parse: string => Js.Json.t

module Serializer {
  module Action {
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

      let fromReasonAction: 'a => t;
    };

    module UserMeta {
      [@bs.deriving abstract]
      type t = {
        [@bs.optional] keys: array(string),
        [@bs.optional] actionName: string
      };
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
  };

  let serializeAction: 'a => Action.t({. "__internal": Action.Internals.t('a), "_type": string}, 'a);
  let serializeObject: 'a => Js.t({.. __internal: Js.Undefined.t(Action.Internals.t('a))});

  let deserializeAction: Action.t({. "__internal": Action.Internals.t('a), "_type": string}, 'a) => 'a;
  let deserializeObject: Js.t({.. __internal: Js.Undefined.t(Action.Internals.t('a))}) => 'b;
};

let labelVariant: ('a, array(string)) => 'a; 
let tagList: list('a) => list('a);
let tagVariant: ('a, string) => 'a;
let tagPolyVar: ('a, string) => 'a;
let tagRecord: ('a, array(string)) => 'a;
