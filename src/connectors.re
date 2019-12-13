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

/* set of Reductive type definitions which are here until available as part of reductive */
type store('action, 'state) = Reductive.Store.t('action, 'state);
type reducer('action, 'state) = ('state, 'action) => 'state;

type middleware('action, 'state) =
  (store('action, 'state), 'action => unit, 'action) => unit;

type storeCreator('action, 'origin, 'state) =
  (
    ~reducer: reducer('action, 'origin),
    ~preloadedState: 'state,
    ~enhancer: middleware('action, 'state)=?,
    unit
  ) =>
  store('action, 'state);

type storeEnhancer('action, 'origin, 'state) =
  storeCreator('action, 'origin, 'state) => storeCreator('action, 'origin, 'state);

type applyMiddleware('action, 'origin, 'state) =
  middleware('action, 'state) => storeEnhancer('action, 'origin, 'state);

let exposeStore: Reductive.Store.t('action, 'state) => t('action, 'state) = store => store |> Obj.magic;

/**
 * originally taken from 
 * https://github.com/reduxjs/redux-devtools/blob/7c3b78d312d3d2b3d4cd8ec9147c68e5c0296b6e/packages/redux-devtools-core/src/utils/index.js#L57-L82
 * 
 * available here until variadic arguments are fixed on redux-devtools-core at 
 * https://github.com/reduxjs/redux-devtools/blob/7c3b78d312d3d2b3d4cd8ec9147c68e5c0296b6e/packages/redux-devtools-core/src/utils/index.js#L81
 */
module ExtensionCoreHelpers {
  let evalMethod: (Extension.Monitor.ActionPayload.t('state, 'action), Js.Dict.t('actionCreator)) => 'action = [%bs.raw {| 
    function evalMethod(action, obj) {
      if (typeof action === 'string') {
        return new Function('return ' + action).call(obj);
      }

      function interpretArg(arg) {
        return new Function('return ' + arg)();
      };

      function evalArgs(inArgs, restArgs) {
        var args = inArgs.map(interpretArg);
        if (!restArgs) return args;
        var rest = interpretArg(restArgs);
        if (Array.isArray(rest)) return args.concat.apply(args, rest);
        throw new Error('rest must be an array');
      };
    
      var args = evalArgs(action.args, action.rest);
      return new Function('...args', 'return this.' + action.name + '(...args)').apply(obj, args);
    }
  |}];
};

module type StateProvider = {
  type t('state, 'action);
  
  let getState: t('state, 'action) => 'state;
  let mutateState: (~state: 'state, ~store: t('state, 'action)) => unit;
  let notifyListeners: t('state, 'action) => unit;
  let dispatch: (~action: 'action, ~store: t('state, 'action)) => unit;
};

type connectionMeta('state, 'action) = {
  /**
   * mutable cached lifted state is used for:
   * 1. sweeping
   * 2. ignoring the jump to those skipped states
   */
  mutable liftedState: option(Extension.Monitor.LiftedState.t('state, 'action)),

  /**
   * record the rewind action index and count
   * so we can drop the dispatched actions when rewinded back
   */
  mutable rewindActionIdx: option(int),
  mutable actionCount: int,
  mutable lastAction: option('action),

  connectionId: string
};

type componentReducer('state, 'action) = ('state, 'action) => 'state;
type componentConnectionInfo('state, 'action) = {
  connection: Extension.connection,
  meta: connectionMeta('state, 'action)
};

let connections: Js.Dict.t(componentConnectionInfo('state, 'action)) = Js.Dict.empty();

module ConnectionHandler = (Store: StateProvider) => {
  open Extension.Monitor;

  module Exceptions {
    exception PayloadNotFound(string);
    exception StateNotFound(string);
    exception ConnectionNotFound(string);
  };

  let _sweepLiftedState = (~devTools: Extension.connection, ~liftedState: LiftedState.t('state, 'action), ~store: Store.t('state, 'action), ~meta: connectionMeta('state, 'action)) => {
    let skippedActions = liftedState |. LiftedState.skippedActionIdsGet;
    let newLiftedState = skippedActions |> Array.fold_left((newLiftedState, _) => { 
      let skipped = newLiftedState |. LiftedState.skippedActionIdsGet |. Array.unsafe_get(0);
      let actionsById = newLiftedState |. LiftedState.actionsByIdGet;
      let newActionsById = Js.Dict.keys(actionsById)
        |. Belt.Array.keep(key => int_of_string(key) != skipped)
        |> Array.map(key => (key, Js.Dict.unsafeGet(actionsById, key)))
        |> Array.fold_left((targetActionsById, (key, value)) => {
          Js.Dict.set(targetActionsById, int_of_string(key) > skipped ? string_of_int(int_of_string(key) - 1) : key, value);
          targetActionsById;
        }, Js.Dict.empty());

      let computedStates = newLiftedState |. LiftedState.computedStatesGet 
        |. Belt.Array.keepWithIndex((_, index) => index != skipped);
      
      let skippedActionIds = newLiftedState |. LiftedState.skippedActionIdsGet
        |. Belt.Array.keep(actionId => actionId != skipped)
        |. Belt.Array.map(entity => entity > skipped ? entity - 1 : entity);

      let stagedActionIds = newLiftedState |. LiftedState.stagedActionIdsGet
        |. Belt.Array.keep(entity => entity != skipped)
        |. Belt.Array.map(entity => entity > skipped ? entity - 1 : entity);
      let currentStateIndex = newLiftedState |. LiftedState.currentStateIndexGet;
      let nextActionId = newLiftedState |. LiftedState.nextActionIdGet;
      
      LiftedState.t(
        ~actionsById=newActionsById, 
        ~computedStates,
        ~currentStateIndex=currentStateIndex > skipped ? currentStateIndex - 1 : currentStateIndex,
        ~nextActionId=nextActionId > skipped ? nextActionId - 1 : nextActionId,
        ~skippedActionIds,
        ~stagedActionIds);
    }, Js.Obj.assign(Js.Obj.empty(), liftedState |> Obj.magic) |> Obj.magic);
    
    meta.actionCount = (newLiftedState |. LiftedState.nextActionIdGet) - 1;
    Extension.send(~connection=devTools, ~action=Js.Null.empty, ~state=newLiftedState);

    /* saw sweeping reseting the time travel if sweeping actions were beyond the time travel point */
    let jumpResetNeeded = Array.fold_left((jumpResetNeeded, skipped) => jumpResetNeeded || (skipped >= (liftedState |. LiftedState.currentStateIndexGet)), false, skippedActions);
    if(jumpResetNeeded){
      let stagedActions = newLiftedState |. LiftedState.stagedActionIdsGet;
      let targetState = newLiftedState |. LiftedState.computedStatesGet
        |. Array.get(Array.length(stagedActions) - 1)
        |. ComputedState.stateGet;

      Store.mutateState(~state=Serializer.deserializeObject(targetState), ~store);
      Store.notifyListeners(store);
    };
  };

  let processToogleAction = (~store: Store.t('action, 'state), ~payload: ActionPayload.t('state, 'action), ~liftedState: LiftedState.t('state, 'action), ~meta: connectionMeta('state, 'action)) => {
    let skippedActions = liftedState |. LiftedState.skippedActionIdsGet;
    let stagedActions = liftedState |. LiftedState.stagedActionIdsGet;
    let computedStates = liftedState |. LiftedState.computedStatesGet;
    let actionsById = liftedState |. LiftedState.actionsByIdGet;

    let id =  payload |. ActionPayload.idGet |> Belt.Option.getExn;
    let idx = Js.Array.indexOf(id, skippedActions);
    let start = Js.Array.indexOf(id, stagedActions);
    let skipped = idx != -1;
    
    if(start === -1){
      liftedState

    } else {
      /* find the first action before new skipped which hasn't been skipped before */
      let initialIdx = ref(start - 1);
      while(Js.Array.includes(initialIdx^, skippedActions)){
        initialIdx := initialIdx^ - 1;
      }

      let initialState = computedStates
        |. Array.get(initialIdx^)
        |. ComputedState.stateGet;

      Store.mutateState(~state=Serializer.deserializeObject(initialState), ~store);
      
      /** a bit hacky */
      let preservedActionCount = meta.actionCount;

      /** 
       * dispatch all actions except already skipped and update lifted state accordingly
       */
      Belt.Array.range((skipped ? start : start + 1), Array.length(stagedActions) - 1)
        /* filter out already skipped actions */
        |. Belt.Array.keep(i => i == start || Js.Array.indexOf(Array.get(stagedActions, i), skippedActions) == -1)
        |> Array.iter(i => {
          let stagedActionKey = string_of_int(Array.get(stagedActions, i));
          let targetAction = Js.Dict.get(actionsById, stagedActionKey)
            |. Belt.Option.getExn
            |. LiftedStateAction.actionGet;
          
          Store.dispatch(~action=Serializer.deserializeAction(targetAction), ~store); 
          let newState = Store.getState(store);
          ComputedState.stateSet(
            Array.get(computedStates, i),
            Serializer.serializeObject(newState));
        });

      /** a bit hacky */
      meta.actionCount = preservedActionCount;

      /* 
       * jump back to correct state during time travel
       * I haven't yet figured out a better way since we still need to dispatch all actions beyond the time travel point
       * to compute correct states after skip adjustment
       */
      if(liftedState |. LiftedState.currentStateIndexGet != Array.length(stagedActions) - 1){
        let targetState = computedStates
        |. Array.get(liftedState |. LiftedState.currentStateIndexGet)
        |. ComputedState.stateGet;

        Store.mutateState(~state=Serializer.deserializeObject(targetState), ~store);
        Store.notifyListeners(store);
      };

      if(skipped){
        let _skippedActions = Js.Array.removeCountInPlace(~pos=idx, ~count=1, skippedActions);
      } else {
        let _skippedActions = Js.Array.push(id, skippedActions);
      }; 

      /** side effect, record the liftedState for:
       * 1. sweeping
       * 2. ignoring the jump to those skipped states 
       */
      meta.liftedState = Some(liftedState);
      liftedState
    };
    
  };

  let processDispatchPayload = (~devTools: Extension.connection, ~store: Store.t('state, 'action), ~payload: ActionPayload.t('state, 'action), ~action: Action.t('state, 'action), ~initial: 'state, ~meta: connectionMeta('state, 'action)) => {
    let payloadType = payload |. ActionPayload.type_Get;
    switch(payloadType) {
      | "LOCK_CHANGES" => {
        /**
         * lock changes are not yet supported, 
         * since it requires sending liftedState back to extension to change the lock state
         * will be available once https://github.com/zalmoxisus/redux-devtools-extension/issues/618#issuecomment-449780372 lands
         */
        ()
      };
      | "COMMIT" => {
        meta.actionCount = 0;
        Extension.init(~connection=devTools, ~state=Serializer.serializeObject(Store.getState(store)));
      };
      | "RESET" => { 
        Store.mutateState(~state=initial, ~store);
        Store.notifyListeners(store);
        meta.actionCount = 0;
        Extension.init(~connection=devTools, ~state=Serializer.serializeObject(Store.getState(store)));
      };
      | "IMPORT_STATE" => {
        let nextLiftedState = payload |. ActionPayload.nextLiftedStateGet |. Belt.Option.getExn;
        let computedStates = nextLiftedState |. LiftedState.computedStatesGet;
        let targetState = Array.get(computedStates, Array.length(computedStates) - 1) |. ComputedState.stateGet;
        
        Store.mutateState(~state=Serializer.deserializeObject(targetState), ~store);
        Store.notifyListeners(store);

        meta.actionCount = (nextLiftedState |. LiftedState.nextActionIdGet) - 1;
        Extension.send(~connection=devTools, ~action=Js.Null.empty, ~state=nextLiftedState);
      };
      | "SWEEP" => {
        /**
         * because of https://github.com/zalmoxisus/redux-devtools-extension/issues/618#issuecomment-449780372
         * SWEEP is not performed on extension side.  
         * 
         * we can perform SWEEP on our side but 
         * Extension is not exposing liftedState to us
         * so we are only able to perform the sweep with the latest cached liftedState
         * that is exposed to us during TOGGLE_ACTION 
         * 
         * this has a disadvantage that any action after liftedState
         * will be lost after sweeping.
         * thus sweeping is left out for now as noop until issue is fixed on extension side
         * 
         * alternatively, liftedState behaviour can be replicated on our side,
         * where sweepLatestLiftedState implementation will be relevant
         */

        /*
        let liftedState = unwrap(meta.liftedState, Exceptions.LiftedStateNotCachedWhileExpected("liftedState hasn't been cached while expected"));
        sweepLiftedState(~devTools, ~liftedState, ~store, ~meta);
        meta.liftedState = None;
        */

        ()
      };
      | "ROLLBACK" => {
        let stateString = action 
          |. Action.stateGet 
          |. unwrap(Exceptions.StateNotFound({j|action($payloadType) doesn't contain state while expected|j}));

        Store.mutateState(~state=Serializer.deserializeObject(Obj.magic(parse(stateString))), ~store);
        Store.notifyListeners(store);
        meta.actionCount = 0;
        Extension.init(~connection=devTools, ~state=Serializer.serializeObject(Store.getState(store)));
      };
      | "JUMP_TO_STATE"
      | "JUMP_TO_ACTION" => {
        /** 
         * if we had already one action skipped before, lifted state is recorded
         * skipped state will be still passed here within the action, in case we hit it
         * find the closest unskipped state before our skipped state
         */
        let stateString = action 
          |. Action.stateGet 
          |. unwrap(Exceptions.StateNotFound({j|action($payloadType) doesn't contain state while expected|j}));
        let actionId = Belt.Option.getExn(payload |. ActionPayload.actionIdGet);
        
        meta.rewindActionIdx = actionId == meta.actionCount ? None : Some(actionId);
        switch(meta.liftedState){
        | Some(liftedState) => {
          let actionInLiftedStateRange = actionId < (liftedState |. LiftedState.nextActionIdGet);
          if(actionInLiftedStateRange){
            let skippedActions = liftedState |. LiftedState.skippedActionIdsGet;
            let computedStates = liftedState |. LiftedState.computedStatesGet;

            let nonSkippedIdx = ref(actionId);
            while(Js.Array.includes(nonSkippedIdx^, skippedActions)){
              nonSkippedIdx := nonSkippedIdx^ - 1;
            };

            let targetState = computedStates
              |. Array.get(nonSkippedIdx^)
              |. ComputedState.stateGet;

            Store.mutateState(~state=Serializer.deserializeObject(targetState), ~store);
          } else {
            Store.mutateState(~state=Serializer.deserializeObject(Obj.magic(parse(stateString))), ~store);
          }
        }
        | None => {
          Store.mutateState(~state=Serializer.deserializeObject(Obj.magic(parse(stateString))), ~store);
        }};

        Store.notifyListeners(store);
      }
      | "TOGGLE_ACTION" => {
        let stateString = action 
          |. Action.stateGet 
          |. unwrap(Exceptions.StateNotFound({j|action($payloadType) doesn't contain state while expected|j}));
        
          /* block the toogle during rewind for now */
          if(
            (Belt.Option.isSome(meta.rewindActionIdx) && Belt.Option.getExn(meta.rewindActionIdx) >= meta.actionCount)
            || Belt.Option.isNone(meta.rewindActionIdx)
          ){
            let liftedState = parse(stateString) |> Obj.magic;
            Extension.send(~connection=devTools, ~action=Js.Null.empty, ~state=processToogleAction(~store=Obj.magic(store), ~payload, ~liftedState, ~meta));
            Store.notifyListeners(store);
          };
      }
      | _ => ()
    };
  }

  let onMonitorDispatch = (~action: Action.t('state, 'action), ~devTools: Extension.connection, ~store: Store.t('state, 'action), ~initial: 'state, ~meta: connectionMeta('state, 'action)) => {
    let payload = action 
      |. Action.payloadGet 
      |. unwrap(Exceptions.PayloadNotFound("action doesn't contain payload while expected"));

    processDispatchPayload(~devTools, ~store, ~payload, ~action, ~initial, ~meta);
  };

  let onRemoteAction = (~action: Action.t('state, 'action), ~store: Store.t('action, 'state), ~actionCreators: option(Js.Dict.t('actionCreator))=?, ()) => {
    let payload = action 
      |. Action.payloadGet 
      |. unwrap(Exceptions.PayloadNotFound("action doesn't contain payload while expected"));

    switch(actionCreators){
    | Some(actionCreators) => Store.dispatch(~action=ExtensionCoreHelpers.evalMethod(payload, Obj.magic(actionCreators)), ~store)
    | None => ()
    };
  };

  let handle = (~connection: Extension.connection, ~store: Store.t('state, 'action), ~meta: connectionMeta('state, 'action), ~actionCreators: option(Js.Dict.t('actionCreator))=?, ()) => {
    let initialState = Store.getState(store); 
    Extension.init(~connection, ~state=Serializer.serializeObject(initialState));

    let _unsubscribe = Extension.subscribe(~connection, ~listener=(action: Action.t('state, 'action)) => {
      switch(action |. Action.type_Get){
      | "DISPATCH" => onMonitorDispatch(~action, ~devTools=connection, ~store, ~initial=initialState, ~meta)
      | "ACTION" => onRemoteAction(~action=Obj.magic(action), ~store=Obj.magic(store), ~actionCreators?, ()) 
      /* do something on start if needed */
      | "START" => ()
      | _ => ()
      };
    });
  };

};

type componentStore('state, 'action) = {
  dispatch: 'action => unit,
  mutable retainedState: 'state,
};

module ComponentStateProvider: StateProvider {
  type t('state, 'action) = componentStore('state, 'action);

  /**
   * component.state is immutable which corresponds to component state at registration
   * for now, resort to retainedState
   */
  let getState = (store: t('state, 'action)) => store.retainedState;
  let mutateState = (~state: 'state, ~store: t('state, 'action)) => {
    /* 
     * we cannot and should not directly mutate a reason-react state
     * instead send a special action `DevToolStateUpdate('state)
     * which then should be handle on the component's reducer side
     */

    store.dispatch(Obj.magic(`DevToolStateUpdate(state)));
  };

  let dispatch = (~action: 'action, ~store: t('state, 'action)) => store.dispatch(action)
  let notifyListeners = _ => ();
};

module ReductiveStateProvider: StateProvider {
  type t('state, 'action) = store('action, 'state);

  let getState = (store: t('state, 'action)) => Reductive.Store.getState(store);
  let mutateState = (~state: 'state, ~store: t('state, 'action)) => {
    exposeStore(store).state = state;
  }

  let dispatch = (~action: 'action, ~store: t('state, 'action)) => Reductive.Store.dispatch(store, action);
  let notifyListeners = (store: t('state, 'action)) => exposeStore(store).listeners |> List.iter(listener => listener());
};

module ComponentConnectionHandler = ConnectionHandler(ComponentStateProvider);
module ReductiveConnectionHandler = ConnectionHandler(ReductiveStateProvider);

/**
 * lock changes and reorder requires liftedState for now
 * unavailable until https://github.com/zalmoxisus/redux-devtools-extension/issues/618#issuecomment-449780372
 * is implemented or we decide to replicate the liftedState on our side
 */
let defaultOptions = connectionId => Extension.enhancerOptions(
  ~name=connectionId,
  ~features=Extension.enhancerFeatures(
    ~lock=false,
    ~pause=true,
    ~persist=true,
    ~export=true,
    ~import=Obj.magic("custom"),
    ~jump=true,
    ~skip=false,
    ~reorder=false,
    ~dispatch=true,
    ()
  ),
  ()
);

let constructOptions: (Extension.enhancerOptions('actionCreator), Extension.enhancerOptions('actionCreator)) => Extension.enhancerOptions('actionCreator) = (options, defaults) => {
  /* combine both first */
  let target = Js.Obj.assign(defaults |> Obj.magic, options |> Obj.magic);
  /* override serialize and make sure unsupported features are disabled */
  Js.Obj.assign(target, {
    "serialize": Extension.serializeOptions(
      ~symbol=true,
      /* ~replacer=Helpers.serializeWithRecordKeys, */
      ()
    ),
    "features": Js.Obj.assign(
      Js.Dict.unsafeGet(Obj.magic(target), "features"), {
        "lock": false,
        "reorder": false,
        "export": true,
        "import": "custom",
      }
    )
  }) |> Obj.magic
};

let nextEnhancer: (~options: Extension.enhancerOptions('actionCreator), ~devToolsUpdateActionCreator: ('state) => 'action) => Types.storeEnhancer('action, 'state) = (~options: Extension.enhancerOptions('actionCreator), ~devToolsUpdateActionCreator) => (storeCreator: Types.storeCreator('action, 'state)) => (~reducer, ~preloadedState, ~enhancer=?, ()) => {
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
  let _didToggle = ref(false);
  let _didInit = ref(false);

  let reduxJsBridgeMiddleware = (store) => {
    let reduxJsStore = switch(_bridgedReduxJsStore^){
    | Some(reduxJsStore) => reduxJsStore
    | None => {
      let bridgedStore = Extension.createDummyReduxJsStore(
        options, 
        (locked) => { _extensionLocked := locked },
        () => { _didToggle := true})(

        // reduxjs reducer bridges reductive updated state 
        (state, action) => { 
          if(_outstandingActionsCount^ == 0){
            /**
              synch reductive state during @@INIT dispatched from monitor (RESET/IMPORT/REVERT) + TOGGLE_ACTION
              no need to resynch @@INIT action on initial mount 
             */
            if(action##"type" == "@@INIT" || _didToggle^){
              _didToggle := false

              if(_didInit^){
                _outstandingActionsCount := (_outstandingActionsCount^ - 1);
                store |. Reductive.Store.dispatch(devToolsUpdateActionCreator(state |> Obj.magic));
              } else {
                _didInit := true
              }
            };

            if(action##"type" != "@@INIT"){
              _outstandingActionsCount := (_outstandingActionsCount^ - 1);
              store |. Reductive.Store.dispatch(Serializer.deserializeAction(action |> Obj.magic));
            }
          };

          store |. Reductive.Store.getState |> Obj.magic
        },
        store |. Reductive.Store.getState |> Obj.magic,
        ()
      );

      bridgedStore.subscribe(() => {
        _outstandingActionsCount := (_outstandingActionsCount^ - 1);        
        if(_outstandingActionsCount^ < 0){
          store |. Reductive.Store.dispatch(devToolsUpdateActionCreator(bridgedStore.getState() |> Obj.magic));
        };
        ()
      });
      
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
          let jsAction = Serializer.serializeAction(action);
          reduxJsStore.dispatch(jsAction |> Obj.magic);
        }
      }
    }
  };

  storeCreator(
    ~reducer, 
    ~preloadedState, 
    ~enhancer=?(Extension.extension == Js.undefined 
      ? enhancer
      : Some(enhancer 
        |. Belt.Option.mapWithDefault(
          reduxJsBridgeMiddleware,
          middleware => (store, next) => reduxJsBridgeMiddleware(store) @@ middleware(store) @@ next
        ))),
    ());
}

let reductiveEnhancer: (Extension.enhancerOptions('actionCreator)) => storeEnhancer('action, 'origin, 'state) = (options: Extension.enhancerOptions('actionCreator)) => (storeCreator: storeCreator('action, 'origin, 'state)) => (~reducer, ~preloadedState, ~enhancer=?, ()) => {
  if(Extension.extension == Js.undefined){
    storeCreator(~reducer, ~preloadedState, ~enhancer?, ());

  } else {
    let targetOptions = constructOptions(options, defaultOptions("ReductiveDevTools"));
    let devTools = Extension.connect(~extension=Extension.devToolsEnhancer, ~options=targetOptions);

    let meta = {
      liftedState: None,
      rewindActionIdx: None,
      actionCount: 0,
      lastAction: None,
      connectionId: targetOptions |. Extension.nameGet
    };

    let devToolsDispatch = (store, next, action) => {
      let propageAction = (store, next, action) => { 
        switch (enhancer) {
          | Some(enhancer) => enhancer(store, next, action)
          | None => next(action)
        };

        meta.actionCount = meta.actionCount + 1;
        Extension.send(~connection=devTools, ~action=Js.Null.return(Serializer.serializeAction(action)), ~state=Serializer.serializeObject(Reductive.Store.getState(store)));
      };

      switch(meta.rewindActionIdx){
      | Some(rewindIdx) => {
        /**
        * do not propage action if rewindActionIdx is set and smaller then last actionIdx
        */
        if(rewindIdx >= meta.actionCount){
          propageAction(store, next, action)
        };
      };
      | _ => propageAction(store, next, action)
      };
    };

    let store: store('action, 'state) = storeCreator(~reducer, ~preloadedState, ~enhancer=devToolsDispatch, ());
    let actionCreators = targetOptions|.Extension.actionCreatorsGet;
    ReductiveConnectionHandler.handle(~connection=devTools, ~store=Obj.magic(store), ~meta, ~actionCreators?, ());
    store;
  }
};

let register = (
  ~connectionId: string, 
  ~component: componentStore('state, 'action),
  ~options: option(Extension.enhancerOptions('actionCreator))=?,
  ()
) => {
  if(Extension.extension == Js.undefined){
    ()
  } else if(Js.Dict.get(connections, connectionId) != None){
    ()
  } else {
    let targetOptions = switch(options){
      | Some(options) => constructOptions(options, defaultOptions(connectionId))
      | None => defaultOptions(connectionId)
      };
    
    let devTools = Extension.connect(~extension=Extension.devToolsEnhancer, ~options=targetOptions);
    let actionCreators = targetOptions|.Extension.actionCreatorsGet;
    
    let connectionInfo = {
      connection: devTools,
      meta: {
        liftedState: None,
        rewindActionIdx: None,
        actionCount: 0,
        lastAction: None,
        connectionId
      }
    };
  
    Js.Dict.set(connections, connectionId, connectionInfo);
    ComponentConnectionHandler.handle(
      ~connection=devTools, 
      ~store=component |> Obj.magic,
      ~meta=connectionInfo.meta |> Obj.magic,
      ~actionCreators?, 
      ());
  }
};

let unsubscribe = (~connectionId: string) => {
  let connectionInfo = Js.Dict.get(connections, connectionId)
    |. unwrap(ComponentConnectionHandler.Exceptions.ConnectionNotFound("DevTool connection(id=$connectionId) not found"));
  
  Js.Dict.unsafeDeleteKey(. connections |> Obj.magic, connectionId);
  Extension.unsubscribe(~connection=connectionInfo.connection);
};

[@bs.val][@bs.scope "console"] 
external warn: 'a => unit = "warn";

[@bs.val][@bs.scope "console"]
external trace: 'a => unit = "trace";

let rewindActionPropagateBlock: (string,
  componentReducer('state, 'a)) =>
    componentReducer('state, 'a) = (connectionId, reducer) => {
  
  (state, action) =>
    switch(Js.Dict.get(connections, connectionId)){
    | None => {
      if(Extension.extension != Js.undefined){
        warn("reductive-dev-tools connection not found while expected");
      };
      
      reducer(state, action);
    }
    | Some(connectionInfo) => {
      switch(connectionInfo.meta.rewindActionIdx){
      | Some(rewindIdx) => {
        /**
         * do not propage action if rewindActionIdx is set and smaller then last actionIdx
         * if it's not a devToolStateUpdate action
         */
        let isDevToolsStateUpdateAction = switch(action){ | `DevToolStateUpdate(_state) => true | _ => false };
        if(rewindIdx >= connectionInfo.meta.actionCount || isDevToolsStateUpdateAction){
          reducer(state, action)
        } else {
          state
        }
      };
      | _ => reducer(state, action)
      };
    }}
};

let captureAction: (string,
  componentReducer('state, ([> `DevToolStateUpdate('state) ] as 'a))) =>
    componentReducer('state, 'a) = (connectionId, reducer) => {
  
  (state, action) =>
    switch(Js.Dict.get(connections, connectionId)){
    | None => {
      if(Extension.extension != Js.undefined){
        warn("reductive-dev-tools connection not found while expected");
      };
      
      reducer(state, action);
    }
    | Some(connectionInfo) => {
      connectionInfo.meta.lastAction = Some(Obj.magic(action));
      reducer(state, action);
    }}
};

let useReducer: (
  Extension.enhancerOptions('actionCreator),
  ('state, ([> `DevToolStateUpdate('state_) ] as 'a)) => 'state, 
  'state
) => ('state, 'a => unit) = (connectionOptions, reducer, initial) => {
  let connectionId = connectionOptions |. Extension.nameGet;
  let targetReducer = React.useMemo1(
    () => rewindActionPropagateBlock(connectionId) @@ captureAction(connectionId) @@ reducer, [|reducer|]);
  let (state, dispatch) = React.useReducer(targetReducer, initial);
  let componentStore = React.useMemo0(() => {
    retainedState: initial,
    dispatch
  });

  React.useEffect0(() => { 
    let retained = ref(initial);
    register(
      ~connectionId, 
      ~component=componentStore, 
      ());
    Some(() => unsubscribe(~connectionId)) 
  });

  /**
    we have to go this way 
    rather then having a reducer wrapping in higher-order reducer(like for reductive case above) 
    that will relay to the extension
    since we cannot ensure that clients will define the reducer outside of component scope,
    in the opossite case internal reacts updateReducer will be called on each render
    which will result in actions dispatched twice to reducer (https://github.com/facebook/react/issues/16295)
   */
  React.useEffect1(() => {
    componentStore.retainedState = state;
    switch(Js.Dict.get(connections, connectionId)){
    | None => if(Extension.extension != Js.undefined){
      warn("reductive-dev-tools connection not found while expected");
    };
    | Some(connectionInfo) => switch(connectionInfo.meta.lastAction){
      /* do not pass DevToolsStateUpdate originated from extension */
      | Some(lastAction) => {
        switch(lastAction){
        | `DevToolStateUpdate(_) => ()
        | _ => {
          connectionInfo.meta.actionCount = connectionInfo.meta.actionCount + 1;
          Extension.send(~connection=connectionInfo.connection, ~action=Js.Null.return(Serializer.serializeAction(lastAction)), ~state=Serializer.serializeObject(state))
        }}
      };
      | None => ()
      };
    };

    None
  }, [|state|]);

  (state, dispatch)
};