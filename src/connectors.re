open Utilities;

/**
 * expose reductive store internals
 * reductiveDevTools mutates the store at times
 */
type t('action, 'state) = {
  mutable state: 'state,
  mutable reducer: ('state, 'action) => 'state,
  mutable listeners: list(unit => unit),
  customDispatcher:
    option((t('action, 'state), 'action => unit, 'action) => unit),
};

type partialStore('action, 'state) = {
  getState: unit => 'state,
  dispatch: 'action => unit
};

type customSerializer('a, 'b) = {
  serialize: 'a => 'b,
  deserialize: 'b => 'a
};

let createDummyReduxJsStore = (options, lockCallback: bool => unit, didToggle: unit => unit) => { 
  let composer = Extension.composeWithDevTools(. options);

  /**
    const store = createStore(reducer, preloadedState, composeEnhancers(
      applyMiddleware(...middleware)
    ));

    the next thing is that applyMiddleware passed inside composedEnhancers
    just assume no other middleware to apply
   */
  let devToolsStoreEnhancer = composer(devToolsEnhancer => devToolsEnhancer);
  let dummyReduxJsStore: ('a, 'b) => ReduxJsStore.t('state, 'action) = (reducer, initial) => {
    let listeners: array(unit => unit) = [||];
    let state = ref(initial);

    let dispatch = (action) => {
      if(action##"type" == "LOCK_CHANGES"){
        lockCallback(action##status)
      } else if(action##"type" == "TOGGLE_ACTION"){
        didToggle();
      };

      let newState = reducer(state^, action);
      state := newState;
      
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

    ReduxJsStore.t(
      ~dispatch,
      ~subscribe=listener => {
        Js.Array.push(listener, listeners) |> ignore;
      },
      ~getState=() => state^,
      ~replaceReducer=_reducer => {
        // noop
        let self = [%bs.raw "this"];
        self
      }
    )
  };

  // let createStore: Types.reduxJsStoreCreator('action, 'state) = [%bs.raw "require('redux').createStore"];
  let rec createStore = (reducer, initial, enhancer) => { 
    switch(enhancer |> Js.toOption){
    | Some(enhancer) => 
      (enhancer @@ (createStore |> Obj.magic))(reducer, initial, ())
    | None => dummyReduxJsStore(reducer, initial)
    }
  };

  (reducer, initial, _enhancer) => createStore(reducer, initial, devToolsStoreEnhancer |> Obj.magic)
};

let createReduxJsBridgeMiddleware = (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~actionSerializer:option(customSerializer('action, 'serializedAction))=?,
  ~stateSerializer:option(customSerializer('state, 'serializedState))=?,
  ~lockCallback:option(bool => unit)=?,
  unit
) => {
  let _bridgedReduxJsStore = ref(None);
  /**
    used to track whether actions have been dispatched
    from reductive store or from monitor

    every action dispatched from reductive will increment
    while every digested action coming from reduxjs store subscription will decrement back
    negative number in below reduxjs subscription will indicate action originate from monitor 
    so dispatch DevToolStateUpdate action to let clients sync reductive state with monitor's
    PLEASE NOTE, reductive reducers need to handle this action themselves
   */
  let _outstandingActionsCount = ref(0);
  let _extensionLocked = ref(false);
  /**
    toggle recalculates: reduces over all states after the one that is toggled
    if it would be accompanied with @@INIT action we won't need to track if toggle was triggered
    as we need to resync the state before dispatching to reductive store all subsequent actions after toggled
   */
  let _justToggled = ref(false);
  let _didInit = ref(false);

  let actionSerializer = actionSerializer |> Obj.magic |. Belt.Option.getWithDefault(Types.({
    serialize: Utilities.Serializer.serializeAction,
    deserialize: Utilities.Serializer.deserializeAction
  }));

  let stateSerializer = stateSerializer |> Obj.magic |. Belt.Option.getWithDefault(Types.({
    serialize: obj => obj,
    deserialize: obj => obj
  }));

  (store: partialStore('action, 'state)) => {
    let reduxJsStore = switch(_bridgedReduxJsStore^){
    | Some(reduxJsStore) => reduxJsStore
    | None => {
      let bridgedStore = createDummyReduxJsStore(
        options, 
        (locked) => { 
          _extensionLocked := locked;
          (lockCallback |. Belt.Option.getWithDefault(_ => ()))(locked)
        },
        () => { _justToggled := true})(

        // reduxjs reducer bridges reductive updated state 
        (state, action) => { 
          if(_outstandingActionsCount^ <= 0){
            /**
              synch reductive state during @@INIT dispatched from monitor (RESET/IMPORT/REVERT) + TOGGLE_ACTION
              no need to resynch @@INIT action on initial mount 
             */
            if(action##"type" == "@@INIT" || _justToggled^){
              _justToggled := false

              if(_didInit^){
                _outstandingActionsCount := (_outstandingActionsCount^ - 1);
                store.dispatch(devToolsUpdateActionCreator(state |> Obj.magic |> stateSerializer.deserialize |> Obj.magic |> Js.Obj.assign(Js.Obj.empty()) |> Obj.magic));
              } else {
                _didInit := true
              }
            };

            if(action##"type" != "@@INIT"){
              _outstandingActionsCount := (_outstandingActionsCount^ - 1);
              store.dispatch(actionSerializer.deserialize(action |> Obj.magic));
            }
          };

          store.getState()
          |> stateSerializer.serialize
          |> Obj.magic
        },

        store.getState()
        |> stateSerializer.serialize
        |> Obj.magic,
        ()
      );

      bridgedStore |. ReduxJsStore.subscribeGet(() => !_extensionLocked^ ? {
        _outstandingActionsCount := (_outstandingActionsCount^ - 1);        
        if(_outstandingActionsCount^ < 0){
          store.dispatch(devToolsUpdateActionCreator(bridgedStore |. ReduxJsStore.getStateGet() |> Obj.magic |> stateSerializer.deserialize |> Obj.magic |> Js.Obj.assign(Js.Obj.empty()) |> Obj.magic));
        };
      } : ());
      
      _bridgedReduxJsStore := Some(bridgedStore);
      bridgedStore
    }};
    
    (next, action) => {
      // discard actions from reductive when monitor is locked
      // still allow actions from monitor though for time-travel
      if(_extensionLocked^ && _outstandingActionsCount^ >= 0){
        ()
      } else {
        next(action)
        
        _outstandingActionsCount := (_outstandingActionsCount^ + 1);
        if(_outstandingActionsCount^ > 0){
          // relay the actions to the reduxjs store
          let jsAction = actionSerializer.serialize(action);
          reduxJsStore |. ReduxJsStore.dispatchGet(jsAction |> Obj.magic);
        }
      }
    }
  };
};

let enhancer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~actionSerializer: customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: customSerializer('state, 'serializedState)=?,
  unit
) => Types.storeEnhancer('action, 'state, 'action, 'state) = (~options, ~devToolsUpdateActionCreator, ~actionSerializer=?, ~stateSerializer=?, ()) => (storeCreator: Types.storeCreator('action, 'state)) => (~reducer, ~preloadedState, ~enhancer=?, ()) => {
  let reduxJsBridgeMiddleware = createReduxJsBridgeMiddleware(~options, ~devToolsUpdateActionCreator, ~actionSerializer?, ~stateSerializer?, ());
  
  storeCreator(
    ~reducer, 
    ~preloadedState, 
    ~enhancer=?(Extension.extension == Js.undefined 
      ? enhancer
      : Some(enhancer 
        |. Belt.Option.mapWithDefault(
          store => reduxJsBridgeMiddleware({ 
            getState: () => Reductive.Store.getState(store), 
            dispatch: Reductive.Store.dispatch(store) 
          }),
          middleware => (store, next) => reduxJsBridgeMiddleware({ 
            getState: () => Reductive.Store.getState(store), 
            dispatch: Reductive.Store.dispatch(store) 
          }) @@ middleware(store) @@ next
        ))),
    ());
}

let captureNextAction = lastAction => reducer => (state, action) => { 
  lastAction := Some(action); 
  reducer(state, action) 
};

let lockReducer = lock => reducer => (state, action) => {
  if(!lock^){
    reducer(state, action)
  } else {
    state
  }
};

let useReducer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~reducer: ('state, 'action) => 'state, 
  ~initial: 'state,
  ~actionSerializer: customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: customSerializer('state, 'serializedState)=?,
  unit
) => ('state, 'action => unit) = (~options, ~devToolsUpdateActionCreator, ~reducer, ~initial, ~actionSerializer=?, ~stateSerializer=?, ()) => {
  let lastAction: ref(option('action)) = React.useMemo0(() => ref(None));
  let extensionLocked = React.useMemo0(() => ref(false));
  if(Extension.extension == Js.undefined){
    React.useReducer(reducer, initial);
  } else {
    let targetReducer = React.useMemo1(() => lockReducer(extensionLocked) @@ captureNextAction(lastAction) @@ reducer, [|reducer|]);
    let (state, dispatch) = React.useReducer(targetReducer, initial); 
    let reduxJsBridgeMiddleware = React.useMemo0(
      () => createReduxJsBridgeMiddleware(~options, ~devToolsUpdateActionCreator, ~actionSerializer?, ~stateSerializer?, ~lockCallback=locked => { extensionLocked := locked }, ())
    );

    let retained = React.useMemo0(() => ref(initial));

    /**
      we have to go this way 
      rather then having a reducer wrapping in higher-order reducer(like for reductive case above) 
      that will relay to the extension
      since we cannot ensure that clients will define the reducer outside of component scope,
      in the opossite case internal reacts updateReducer will be called on each render
      which will result in actions dispatched twice to reducer (https://github.com/facebook/react/issues/16295)
    */
    retained := state;
    React.useEffect1(() => {
      let middleware = reduxJsBridgeMiddleware({
        getState: () => retained^,
        dispatch
      });

      switch(lastAction^){
      | Some(action) => middleware((_action) => (), action)
      | _ => ()
      };
      
      None
    }, [|state|]);

    (state, dispatch)
  }
};