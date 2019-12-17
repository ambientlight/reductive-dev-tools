## reductive-dev-tools

[![VERSION](https://img.shields.io/npm/v/reductive-dev-tools)](https://www.npmjs.com/package/reductive-dev-tools)
[![LICENSE](https://img.shields.io/github/license/ambientlight/reductive-dev-tools)](https://github.com/ambientlight/reductive-dev-tools/blob/master/LICENSE)
[![ISSUES](https://img.shields.io/github/issues/ambientlight/reductive-dev-tools)](https://github.com/ambientlight/reductive-dev-tools/issues)

[reductive](https://github.com/reasonml-community/reductive) and [reason-react](https://github.com/reasonml/reason-react) reducer component integration with [redux-devtools-extension](https://github.com/zalmoxisus/redux-devtools-extension)

![image](assets/demo.gif)

## Installation 

1. with npm: 
  ```bash
  npm install --save-dev reductive-dev-tools
  ```

2. add `reductive-dev-tools` to your "bs-dependencies" inside `bsconfig.json`.
3. add `-bs-g` into `"bsc-flags"` of your **bsconfig.json** to have variant and record field names available inside extension.

**Peer depedencies**  
reason-react, reductive, redux-devtools-extension, redux (redux-devtools-extension's peer dep.) should be also installed.

## Usage
Utilize provided store enhancer `ReductiveDevTools.Connectors.enhancer` for **reductive** or `ReductiveDevTools.Connectors.useReducer` for **reason-react hooks** (jsx3). 

You need to pass devtools extension [options](#options) as `~options` and action creator that builds action when state update is dispatched from the monitor as `~devToolsUpdateActionCreator`. Additionally you can also pass `~stateSerializer` and `~actionSerializer` to override default serialization behaviour. Take a look at [Serialization](#serialization) to see if you need it.

#### reductive

```reason
let storeCreator = 
  ReductiveDevTools.Connectors.enhancer(
    ~options=ReductiveDevTools.Extension.enhancerOptions(
      ~name=__MODULE__, 
      ~actionCreators={
        "actionYouCanDispatchFromMonitor": (value: int) => `YourActionOfChoice(value)
          |. ReductiveDevTools.Utilities.Serializer.serializeAction
      },
      ()),
    ~devToolsUpdateActionCreator=(devToolsState) => `DevToolsUpdate(devToolsState),
    ()
  ) 
  @@ Reductive.Store.create;
```

#### React Hooks useReducer (jsx3)

```reason
let (state, send) = ReductiveDevTools.Connectors.useReducer(
  ~options=ReductiveDevTools.Extension.enhancerOptions(
    ~name=__MODULE__, 
    ~actionCreators={
      "actionYouCanDispatchFromMonitor": (value: int) => `YourActionOfChoice(value)
        |. ReductiveDevTools.Utilities.Serializer.serializeAction
    },
    ()),
  ~devToolsUpdateActionCreator=(devToolsState) => `DevToolsUpdate(devToolsState),
  ~reducer,
  ~initial=yourInitialState,
  ());
```

#### Usage with ReactReason legacy reducer component (jsx2)
No longer supported. Please install latest from 0.x:

```
npm install --save-dev reductive-dev-tools@0.2.6
```

And refer to [old documentation](https://github.com/ambientlight/reductive-dev-tools/blob/dac77af64763d1aaed584a405c8caeb8b8597272/README.md#usage-with-reactreason-reducer-component).

## Serialization

### Actions
[redux-devtools-extension](https://github.com/zalmoxisus/redux-devtools-extension) uses value under `type` key of action object for its name in the monitor. Most likely you are going to use [variants](https://reasonml.github.io/docs/en/variant) for actions, which need to be serialized into js objects to be usefully displayed inside the extension. Actions serialization is built-in. As an alternative, you can override default serializer by passing `~actionSerializer` like:

```reason
ReductiveDevTools.Connectors.enhancer(
  ~options=ReductiveDevTools.Extension.enhancerOptions(
    ~name=__MODULE__, 
    ()),
  ~actionSerializer={
    serialize: obj => {
      // your serialization logic
      obj
    },
    deserialize: obj => {
      // your deserialization logic
      obj
    }
  },
  ())
```

There are few caveats that apply to default serialization though.

1. Make sure to add `-bs-g` into `"bsc-flags"` of your **bsconfig.json** to have variant names available.
2. Variants with constructors should be prefered to plain (`SomeAction(unit)` to `SomeAction`) since plain varaints do no carry debug metedata(in symbols) with them (represented as numbers in js).
3. Action names won't be displayed when using [extensible variants](https://caml.inria.fr/pub/docs/manual-ocaml/manual037.html#sec269), they also do not carry debug metadata. [Extensible variant name becomes "update"](https://github.com/ambientlight/reductive-dev-tools/issues/2)
4. Records inside variants do not carry debug metadata in bucklescript yet, if needed you can tag them manually. See [Additional Tagging](#additional-tagging).

### State

There is no serialization applied to state by default. If you are on [bs-platform 7.0](https://github.com/BuckleScript/bucklescript/releases/tag/7.0.1), most likely you do not need it, since [ocaml records are compiled to js objects](https://bucklescript.github.io/blog/2019/11/18/whats-new-in-7). For earlier versions of [bs-platform](https://www.npmjs.com/package/bs-platform), please pass the next `~stateSerializer`:

```reason
ReductiveDevTools.Connectors.enhancer(
  ~options=ReductiveDevTools.Extension.enhancerOptions(
    ~name=__MODULE__, 
    ()),
  ~stateSerializer={
    serialize: obj => obj |. ReductiveDevTools.Utilities.Serializer.serializeObject,
    deserialize: obj => obj |. ReductiveDevTools.Utilities.Serializer.deserializeObject
  },
  ())
```

## Options

```reason
ReductiveDevTools.Extension.enhancerOptions(
  /* the instance name to be showed on the monitor page */
  ~name="SomeTest",
  
  /* action creators functions to be available in the Dispatcher. */
  ~actionCreators={
    "increment": () => `Increment(()) |. ReductiveDevTools.Utilities.Serializer.serializeAction,
    "decrement": () => `Decrement(()) |. ReductiveDevTools.Utilities.Serializer.serializeAction
  },
  
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

## Additional Tagging
You can also manually customize serialized objects keys and action names displayed inside extension.
Two common usecases:

1. Labeling variants with constructors.

	```reason
	type routerActions = [
	  | `RouterLocationChanged(list(string), string, string)
	];
	
	open ReductiveDevTools.Utilities;
	Reductive.Store.dispatch(store, 
	  `RouterLocationChanged(url.path, url.hash, url.search)
	    |. labelVariant([|"path", "hash", "search"|]));
	```
2. Labeling record keys for records inside variants (since Records inside variants do not carry debug metadata in bucklescript yet).

	```reason
	type url = {
	  path: list(string),
	  hash: string,
	  search: string,
	};
	type routerActions = [
	  | `RouterLocationChanged(url)
	];
	
	open ReductiveDevTools.Utilities;
	Reductive.Store.dispatch(store, 
	  `RouterLocationChanged(url
	    |. tagRecord([|"path", "hash", "search"|]));
	```
	
This can also be used to override bucklescript debug metadata(if really needed). Definitions are at: [utilities.rei](https://github.com/ambientlight/reductive-dev-tools/blob/a530ea6d09d7facad2b70c061703eff52cfa80b4/src/utilities.rei#L63-L67)
