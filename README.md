## reductive-dev-tools

[reductive](https://github.com/reasonml-community/reductive) and [reason-react](https://github.com/reasonml/reason-react) reducer component integration with Redux DevTools.

![image](assets/demo.gif)

## Installation 
via npm:

```bash
npm install --save-dev reductive-dev-tools
```
then add `reductive-dev-tools` to your "bs-dependencies" inside `bsconfig.json`

## Caveats

1. Add `-bs-g` into `"bsc-flags"` of your **bsconfig.json** to have variant and record field names available inside extension.
2. Prefer variants with associated data to plain (`SomeAction(unit)` to `SomeAction`) since plain varaints do no carry debug metedata with them (represented as numbers in js)

## Supported DevTools Features

| feature | reductive | reducer component |
|---------|-----------|-------------------|
| pause   | ✔         | ✔                 |
| lock    |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| persist | ✔         | ✔                 |
| export  | ✔         | ✔                 |
| import  | ✔         | ✔                 |
| jump    | ✔         | ✔                 |
| skip    | ✔         | [#1](https://github.com/ambientlight/reductive-dev-tools/issues/1)|
| sweep |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| reorder |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| dispatch| ✔         | ✔                 |
| test    | REDUX ONLY| REDUX ONLY        |
| trace   | REDUX ONLY| REDUX ONLY        | 

## Usage with Reductive
Utilize provided store enhancer `ReductiveDevTools.Connectors.reductiveEnhancer`

```reason
let storeEnhancer =
  ReductiveDevTools.(
    Connectors.reductiveEnhancer(
      Extension.enhancerOptions(~name="MyApp", ()),
    )
  );
  
let storeCreator = storeEnhancer @@ Reductive.Store.create;
```

## Experimental: Direct API with ReasonReact reducer component

1. Create devtools connection with `ReductiveDevTools.Connectors.register()`.

	```reason
	/* in your component */
	didMount: self =>
	  ReductiveDevTools.Connectors.register(
	    ~connectionId, 
	    ~component=self, 
	    ~options=ReductiveDevTools.Extension.enhancerOptions(
	      ~actionCreators=Js.Dict.fromList([
	        ("click", (.) => `Click()),
	        ("toogle", (.) => `Toggle(()))
	      ]),
	      ()), 
	    ()),
	```
2. Retain an action... inside your component state... (yeah...)
	
	```reason
	/* State declaration */
	type state = {
	  count: int,
	  show: bool,
	
	  preserved: preservedAction
	} and preservedAction = {
	  action: option([`Click(unit) | `Toggle(unit) | `DevToolStateUpdate(state)])
	};
	
	/* your reducer */
	reducer: (action, state) => {
	  let stateWithAction = {...state, preserved: { action: Some(action) }};
	  switch (action) {
	  | `Click(_) => ReasonReact.Update({...stateWithAction, count: state.count + 1})
	  | `Toggle(_) => ReasonReact.Update({...stateWithAction, show: !state.show})
	  | `DevToolStateUpdate(devToolsState) => ReasonReact.Update({...devToolsState, preserved: { action: Some(action) }})
	  }
	},
	```

3. Send new state and action with `ReductiveDevTools.Connectors.send()`

	```reason
	/* in your component */
	willUpdate: ({newSelf}) =>
	  switch(newSelf.state.preserved.action){
	  | Some(action) => ReductiveDevTools.Connectors.send(~connectionId, ~action, ~state=newSelf.state);
	  | None => ()
	  },
	```
	
4. Handle actions dispatched from the monitor (`DevToolStateUpdate('state)`)

	```reason
	/* your reducer */
	reducer: (action, state) => {
	  let stateWithAction = {...state, preserved: { action: Some(action) }};
	  switch (action) {
	  /* other actions */
	  | `DevToolStateUpdate(devToolsState) => ReasonReact.Update({...devToolsState, preserved: { action: Some(action) }})
	  }
	},
	```
	
5. Unsubscribe when needed with `ReductiveDevTools.Connectors.unsubscribe(~connectionId)`

## Options

```reason
ReductiveDevTools.Extension.enhancerOptions(
  /* the instance name to be showed on the monitor page */
  ~name="SomeTest",
  
  /* action creators functions to be available in the Dispatcher. */
  ~actionCreators=Js.Dict.fromList([
    ("increment", (.) => CounterAction(Increment)),
    ("decrement", (.) => CounterAction(Decrement)),
    ("complexAppend", ((first, last) => StringAction(ComplexAppend(first, last))) |> Obj.magic)
  ]),
  
  /* if more than one action is dispatched in the indicated interval, all new actions will be collected and sent at once */
  ~latency=500,
  
  /* maximum allowed actions to be stored in the history tree. */
  ~maxAge=50,
  
  /* actions types to be hidden / shown in the monitors (while passed to the reducers), If `actionsWhitelist` specified, `actionsBlacklist` is ignored. */
  ~actionsBlacklist=[|"StringAction"|],
  
  /* actions types to be hidden / shown in the monitors (while passed to the reducers), If `actionsWhitelist` specified, `actionsBlacklist` is ignored. */
  ~actionsWhitelist=[|"CounterAction"|],
  
  /* if specified as `true`, whenever there's an exception in reducers, the monitors will show the error message, and next actions will not be dispatched. */
  ~shouldCatchErrors=false,
  
  /* If you want to restrict the extension, specify the features you allow. */
  ~features=ReductiveDevTools.Extension.enhancerFeatures(
    ~pause=true,
    ~persist=true,
    ~export=true,
    ~import=Obj.magic("custom"),
    ~jump=true,
    ~dispatch=true,
    ()),
  ())
```

## Word Of Caution
Current implementation depends on internal bucklescript representation of debug metadata and variants in js. Changes to it in future may silently break the extension.