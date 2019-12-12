[@bs.deriving abstract]
type serializeOptions = {
  [@bs.optional] date: bool,
  [@bs.optional] regex: bool,
  [@bs.optional] undefined: bool,
  [@bs.optional] error: bool,
  [@bs.optional] symbol: bool,
  [@bs.optional] map: bool,
  [@bs.optional] set: bool,
  [@bs.optional][@bs.as "function"] function_: bool,

  [@bs.optional] replacer: (string, Js.t({.})) => Js.t({.})
};

[@bs.deriving abstract]
type enhancerFeatures = {
  /**
    * start/pause recording of dispatched actions
    */
  [@bs.optional] pause: bool,
  /**
    * lock/unlock dispatching actions and side effects
    */
  [@bs.optional] lock: bool,
  /**
    * persist states on page reloading
    */
  [@bs.optional] persist: bool,

  /** https://github.com/BuckleScript/bucklescript/issues/2934 */
  /* [@bs.optional][@bs.unwrap] export: [
    | `Bool(bool)
    | `String(string)
  ], */

  /**
    * export history of actions in a file
    * bool or "custom"
    * since we cannot use polymorhic variant and [@bs.unwrap] here,
    * pass "custom" as Obj.magic("custom") if needed
    */
  [@bs.optional] export: bool,

  /** https://github.com/BuckleScript/bucklescript/issues/2934 */
  /* [@bs.optional][@bs.unwrap] import: [
    | `Bool(bool)
    | `String(string)
  ], */

  /**
    * import history of actions from a file
    * bool or "custom"
    * since we cannot use polymorhic variant and [@bs.unwrap] here,
    * pass "custom" as Obj.magic("custom") if needed
    */
  [@bs.optional] import: bool,
  /**
    * jump back and forth (time travelling)
    */
  [@bs.optional] jump: bool,
  /**
    * skip (cancel) actions
    */
  [@bs.optional] skip: bool,
  /**
    * drag and drop actions in the history list
    */
  [@bs.optional] reorder: bool,
  /**
    * dispatch custom actions or action creators
    */
  [@bs.optional] dispatch: bool,
  
  /**
    * generate tests for the selected actions
    *
    * NOT AVAILABLE FOR NON-REDUX
    */
  [@bs.optional] test: bool
};

type extension = Js.t({.});
type connection = Js.t({.});

[@bs.deriving abstract]
type enhancerOptions('actionCreator) = {
  /**
   * the instance name to be showed on the monitor page. Default value is `document.title`.
   */
  name: string,
  
  /**
   * action creators functions to be available in the Dispatcher.
   */
  [@bs.optional] actionCreators: Js.Dict.t('actionCreator),

  /**
   * if more than one action is dispatched in the indicated interval, all new actions will be collected and sent at once.
   * It is the joint between performance and speed. When set to `0`, all actions will be sent instantly.
   * Set it to a higher value when experiencing perf issues (also `maxAge` to a lower value).
   *
   * @default 500 ms.
   */
  [@bs.optional] latency: int,

  /**
   * (> 1) - maximum allowed actions to be stored in the history tree. The oldest actions are removed once maxAge is reached. It's critical for performance.
   *
   * @default 50
   */
  [@bs.optional] maxAge: int,

  /**
   * - `undefined` - will use regular `JSON.stringify` to send data (it's the fast mode).
   * - `false` - will handle also circular references.
   * - `true` - will handle also date, regex, undefined, error objects, symbols, maps, sets and functions.
   * - object, which contains `date`, `regex`, `undefined`, `error`, `symbol`, `map`, `set` and `function` keys.
   *   For each of them you can indicate if to include (by setting as `true`).
   *   For `function` key you can also specify a custom function which handles serialization.
   *   See [`jsan`](https://github.com/kolodny/jsan) for more details.
   */
  [@bs.optional] serialize: serializeOptions,

  /**
   * function which takes `action` object and id number as arguments, and should return `action` object back.
   */
  /* [@bs.optional] actionSanitizer: (. 'action) => 'action, */

  /**
   * function which takes `state` object and index as arguments, and should return `state` object back.
   */
  /* [@bs.optional] stateSanitizer: (. 'state) => 'state, */

  /**
   * *string or array of strings as regex* - actions types to be hidden / shown in the monitors (while passed to the reducers).
   * If `actionsWhitelist` specified, `actionsBlacklist` is ignored.
   */
  [@bs.optional] actionsBlacklist: array(string),

  /**
   * *string or array of strings as regex* - actions types to be hidden / shown in the monitors (while passed to the reducers).
   * If `actionsWhitelist` specified, `actionsBlacklist` is ignored.
   */
  [@bs.optional] actionsWhitelist: array(string),

  /**
   * called for every action before sending, takes `state` and `action` object, and returns `true` in case it allows sending the current data to the monitor.
   * Use it as a more advanced version of `actionsBlacklist`/`actionsWhitelist` parameters.
   */
  /* [@bs.optional] predicate: (. 'state, 'action) => bool, */

  /**
   * if specified as `false`, it will not record the changes till clicking on `Start recording` button.
   * Available only for Redux enhancer, for others use `autoPause`.
   *
   * @default true
   */
  [@bs.optional] shouldRecordChanges: bool,

  /**
   * if specified, whenever clicking on `Pause recording` button and there are actions in the history log, will add this action type.
   * If not specified, will commit when paused. Available only for Redux enhancer.
   */
  [@bs.optional] pauseActionType: bool,

  /**
   * auto pauses when the extension’s window is not opened, and so has zero impact on your app when not in use.
   * Not available for Redux enhancer (as it already does it but storing the data to be sent).
   *
   * @default false
   */
  [@bs.optional] autoPause: bool,

  /**
   * if specified as `true`, it will not allow any non-monitor actions to be dispatched till clicking on `Unlock changes` button.
   * Available only for Redux enhancer.
   *
   * @default false
   */
  [@bs.optional] shouldStartLocked: bool,

  /**
   * if set to `false`, will not recompute the states on hot reloading (or on replacing the reducers). Available only for Redux enhancer.
   *
   * @default true
   */
  [@bs.optional] shouldHotReload: bool,

  /**
   * if specified as `true`, whenever there's an exception in reducers, the monitors will show the error message, and next actions will not be dispatched.
   *
   * @default false
   */
  [@bs.optional] shouldCatchErrors: bool,

  /**
   * If you want to restrict the extension, specify the features you allow.
   * If not specified, all of the features are enabled. When set as an object, only those included as `true` will be allowed.
   * Note that except `true`/`false`, `import` and `export` can be set as `custom` (which is by default for Redux enhancer), meaning that the importing/exporting occurs on the client side.
   * Otherwise, you'll get/set the data right from the monitor part.
   */
  [@bs.optional] features: enhancerFeatures,

  /**
   * if set to true, will include stack trace for every dispatched action, so you can see it in trace tab jumping directly to that part of code
   * 
   * NOT AVAILABLE FOR NON REDUX
   */
  [@bs.optional] trace: bool,

  /**
   * maximum stack trace frames to be stored (in case trace option was provided as true). 
   * By default it's 10. Note that, because extension's calls are excluded, the resulted frames could be 1 less. 
   * If trace option is a function, traceLimit will have no effect, as it's supposed to be handled there.
   * 
   * NOT AVAILABLE FOR NON REDUX
   */
  [@bs.optional] traceLimit: int  
};

[@bs.val] [@bs.scope "window"]
external extension: Js.Undefined.t(extension) = "__REDUX_DEVTOOLS_EXTENSION__";

[@bs.val] [@bs.scope "window"]
external devToolsExtensionLocked: bool = "__REDUX_DEVTOOLS_EXTENSION_LOCKED__";

[@bs.module "redux-devtools-extension"]
external _devToolsEnhancer: extension = "devToolsEnhancer";

type composer('action, 'state) = Types.reduxJsStoreEnhancer('action, 'state) => Types.reduxJsStoreCreator('action, 'state) => Types.reduxJsStoreCreator('action, 'state);

[@bs.module "redux-devtools-extension"]
external _composeWithDevTools: (. enhancerOptions('actionCreator)) => composer('action, 'state) = "composeWithDevTools";

let createDummyReduxJsStore = options => { 
  let composer = _composeWithDevTools(. options);

  /**
    const store = createStore(reducer, preloadedState, composeEnhancers(
      applyMiddleware(...middleware)
    ));

    the next thing is that applyMiddleware passed inside composedEnhancers
    just assume no other middleware to apply
   */
  let devToolsStoreEnhancer = composer(devToolsEnhancer => devToolsEnhancer);
  let dummyReduxJsStore: ('a, 'b) => Types.reduxJsStore('state, 'action) = (reducer, initial) => {
    let listeners: array(unit => unit) = [||];
    let state = ref(initial);

    let dispatch = (action) => {
      // Js.log("reduxjs-dispatch");
      // Js.log(action);
      let newState = reducer(state^, action);
      state := newState;
      // Js.log(newState);
      
      listeners
      |. Belt.Array.forEach(listener => listener());

      state^
    };

    /**
      if we don't mimic reduxjs's initial action devtools seem to lag the resulting state by 1
      since redux-dev-tools seems to fake the initial @@redux/INIT action 
      (we will see it in the monitor even if the thing below is not dispatched)
     */ 
    dispatch({ "type": "@@redux/INIT"} |> Obj.magic);

    {
      dispatch,
      subscribe: listener => {
        Js.Array.push(listener, listeners) |> ignore;
        ()
      },
      getState: () => {
        // represents monitor's liftedState
        state^
      },
      replaceReducer: _reducer => {
        // noop
        let self = [%bs.raw "this"];
        self
      }
    }
  };

  let rec createStore = (reducer, initial, enhancer) => { 
    switch(enhancer |> Js.toOption){
    | Some(enhancer) => 
      (enhancer @@ (createStore |> Obj.magic))(reducer, initial, ())
    | None => dummyReduxJsStore(reducer, initial)
    }
  };

  (reducer, initial, _enhancer) => createStore(reducer, initial, devToolsStoreEnhancer |> Obj.magic)
};

let devToolsEnhancer = _devToolsEnhancer;

[@bs.send] external _connect: (~extension: extension, ~options: enhancerOptions('actionCreator)) => connection = "connect";
[@bs.send] external _disconnect: (~extension: extension) => unit = "disconnect";
[@bs.send] external _send: (~extension: extension, ~action: 'action, ~state: 'state, ~options: enhancerOptions('actionCreator)=?, ~instanceId: string=?, unit) => unit = "send";
[@bs.send] external _listen: (~extension: extension, ~onMessage: 'action => unit, ~instanceId: string) => unit = "listen";
[@bs.send] external _open: (~extension: extension, ~position: string=?, unit) => unit = "open";
[@bs.send] external _notifyErrors: (~onError: (. Js.t({..}) ) => unit=?, unit) => unit = "notifyErrors";

let connect = (~extension: extension, ~options: enhancerOptions('actionCreator)) => _connect(~extension, ~options);

/**
 * Remove extensions listener and disconnect extensions background script connection. 
 * Usually just unsubscribing the listiner inside the connect is enough.
 */
let disconnect = (~extension: extension) => _disconnect(~extension);

/**
 * Send a new action and state manually to be shown on the monitor. 
 * It's recommended to use connect, unless you want to hook into an already created instance.
 */
let send = (~extension: extension, ~action: 'action, ~state: 'state, ~options=?, ~instanceId=?, _:unit) => _send(~extension, ~action, ~state, ~options?, ~instanceId?, ());

/**
 * Listen for messages dispatched for specific instanceId. 
 * For most of cases it's better to use subcribe inside the connect.
 */
let listen = (~extension: extension, ~onMessage: 'action => unit, ~instanceId: string) => _listen(~extension, ~onMessage, ~instanceId);

/**
 * Open the extension's window. 
 * This should be conditional (usually you don't need to open extension's window automatically).
 */
let open_ = (~extension: extension, ~position=?, _:unit) => _open(~extension, ~position?, ());

/**
 * When called, the extension will listen for uncaught exceptions on the page, and, if any, will show native notifications. 
 * Optionally, you can provide a function to be called when and exception occurs.
 */
let notifyErrors = (~onError=?, _: unit) => _notifyErrors(~onError?, ());


[@bs.send] external _subscribe: (~connection: connection, ~listener: 'action => unit) => (. unit) => unit = "subscribe";
[@bs.send] external _unsubscribe: (~connection: connection) => unit = "unsubscribe";
[@bs.send] external _send: (~connection: connection, ~action: Js.Null.t('action), ~state: 'state) => unit = "send";
[@bs.send] external _init: (~connection: connection, ~state: 'state) => unit = "init";
[@bs.send] external _error: (~connection: connection, ~message: string) => unit = "error";

/**
 * adds a change listener. It will be called any time an action is dispatched form the monitor. 
 * Returns a function to unsubscribe the current listener.
 */
let subscribe = (~connection: connection, ~listener: 'action => unit) => _subscribe(~connection, ~listener);

/**
 * unsubscribes all listeners.
 */
let unsubscribe = (~connection: connection) => _unsubscribe(~connection);

/**
 * sends a new action and state manually to be shown on the monitor. 
 * If action is null then we suppose we send liftedState.
 */
let send = (~connection: connection, ~action: Js.Null.t('action), ~state: 'state) => _send(~connection, ~action, ~state);
/**
 * sends the initial state to the monitor
 */
let init = (~connection: connection, ~state: 'state) => _init(~connection, ~state);
/**
 * sends the error message to be shown in the extension's monitor.
 */
let error = (~connection: connection, ~message: string) => _error(~connection, ~message);


module Monitor {
  module LiftedStateAction {
    [@bs.deriving abstract]
    type t('action) = {
      action: 'action,
      timestamp: int,
      [@bs.as "type"] type_: string
    };
  };

  module ComputedState {
    [@bs.deriving abstract]
    type t('state) = {
      mutable state: 'state
    };
  };

  module LiftedState {
    [@bs.deriving abstract]
    type t('state, 'action) = {
      actionsById: Js.Dict.t(LiftedStateAction.t('action)),
      computedStates: array(ComputedState.t('state)),
      currentStateIndex: int,
      nextActionId: int,
      skippedActionIds: array(int),
      stagedActionIds: array(int)
    };
  };

  module ActionPayload {
    [@bs.deriving abstract]
    type t('state, 'action) = {
      [@bs.as "type"] type_: string,
      [@bs.optional] timestamp: int,
      [@bs.optional] id: int,
      [@bs.optional] actionId: int,
      [@bs.optional] index: int,
      [@bs.optional] nextLiftedState: LiftedState.t('state, 'action)
    };
  };

  module Action {
    [@bs.deriving abstract]
    type t('state, 'action) = {
      [@bs.as "type"] type_: string,
      [@bs.optional] payload: ActionPayload.t('state, 'action),
      [@bs.optional] state: string
    };
  };
};