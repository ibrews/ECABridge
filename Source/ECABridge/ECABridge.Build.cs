// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ECABridge : ModuleRules
{
	// Returns true if the engine ships the named plugin (search Engine/Plugins recursively
	// for <PluginName>.uplugin). Used to gate optional feature subsystems so that ECABridge
	// can be built against engines that don't have e.g. Mutable or MovieRenderPipeline.
	//
	// Note: this answers "is the plugin present in the engine on disk", which is the right
	// question for distinguishing a stock UE install from a custom engine fork that compiled
	// out a plugin. It does NOT answer "did the user enable this plugin in their .uproject"
	// — Target.Plugins isn't exposed on ReadOnlyTargetRules in 5.8, and reading the .uproject
	// from Build.cs is too fragile. Users who want to opt out of a subsystem despite the
	// engine having it can either disable the plugin in their .uproject (the .uplugin
	// references are "Optional": true so UE won't error) OR force-override the WITH_ECA_*
	// preprocessor symbols here.
	private bool EngineHasPlugin(string PluginName)
	{
		string PluginsDir = Path.Combine(EngineDirectory, "Plugins");
		if (!Directory.Exists(PluginsDir)) return false;
		// .NET 8 refuses to traverse "untrusted" reparse points (e.g. PixelStreaming's
		// node_modules pnpm symlinks under Engine/Plugins/Media/PixelStreaming/Resources).
		// Skip reparse points + inaccessible entries so the scan can't be tripped by an
		// engine plugin that ships with junctioned subtrees.
		var opts = new System.IO.EnumerationOptions
		{
			RecurseSubdirectories = true,
			AttributesToSkip = System.IO.FileAttributes.ReparsePoint,
			IgnoreInaccessible = true,
		};
		return Directory.GetFiles(PluginsDir, PluginName + ".uplugin", opts).Length > 0;
	}

	// Adds a delay-load DLL entry on Windows only. PublicDelayLoadDLLs is a
	// Windows-specific mechanism — on Mac/Linux the module linker resolves
	// optional plugin dylibs through PrivateDependencyModuleNames automatically.
	// Adding .dll names on Mac causes the linker to search for them as libraries,
	// producing "library not found" errors with wrong Engine/Source/ paths.
	private void AddDelayLoadDLL(string dll)
	{
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDelayLoadDLLs.Add(dll);
		}
	}

	// Returns true only when the plugin exists AND has compiled binaries for the current
	// platform. On Mac, launcher installs omit binaries for many engine plugins (Mutable,
	// nDisplay, etc.) even though the .uplugin and source are present. Without binaries
	// the UHT-generated headers are missing and the link step fails. Use this instead of
	// EngineHasPlugin for any optional dep whose module is linked (not just header-only).
	private bool EngineHasPluginWithBinaries(string PluginName)
	{
		string PluginsDir = Path.Combine(EngineDirectory, "Plugins");
		if (!Directory.Exists(PluginsDir)) return false;
		var opts = new System.IO.EnumerationOptions
		{
			RecurseSubdirectories = true,
			AttributesToSkip = System.IO.FileAttributes.ReparsePoint,
			IgnoreInaccessible = true,
		};
		string[] upluginFiles = Directory.GetFiles(PluginsDir, PluginName + ".uplugin", opts);
		if (upluginFiles.Length == 0) return false;
		string PluginDir = Path.GetDirectoryName(upluginFiles[0]);
		string BinariesDir = Path.Combine(PluginDir, "Binaries");
		return Directory.Exists(BinariesDir) &&
		       Directory.GetFiles(BinariesDir, "*", opts).Length > 0;
	}

	public ECABridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp23;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "EngineSettings", "Json", "JsonUtilities",
			"HTTPServer",  // For MCP HTTP/SSE server
			"Sockets",     // For ISocketSubsystem
			"Networking"   // For networking utilities
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Editor core
			"UnrealEd",

			// Mesh import/manipulation
			"MeshDescription", "StaticMeshDescription",
			// GeometryScript - Modern procedural mesh API
			"GeometryCore", "GeometryFramework", "GeometryScriptingCore", "DynamicMesh", "MeshConversion",
			// Modeling tools subdivision
			"ModelingComponentsEditorOnly", "ModelingOperators",
			// ProceduralMeshComponent - Runtime procedural meshes
			"ProceduralMeshComponent",

			"EditorSubsystem", "EditorFramework", "LevelEditor",

			// UI/Slate
			"Slate", "SlateCore", "UMG", "UMGEditor", "PropertyEditor", "InputCore",
			// MVVM ViewModel binding
			"ModelViewViewModel", "ModelViewViewModelBlueprint", "ModelViewViewModelEditor",

			// Blueprint manipulation
			"BlueprintGraph", "Kismet", "KismetCompiler", "GraphEditor",

			// Asset management
			"AssetTools", "AssetRegistry", "ContentBrowser", "EditorScriptingUtilities",  // For EditorAssetLibrary
			"MaterialEditor",            // For material editing utilities
			"DataTableEditor",           // For DataTable editor UI refresh (BroadcastPostChange)

			// Additional utilities
			"Projects", "DeveloperSettings", "ToolMenus",

			// Image processing
			"ImageWrapper",

			// Interchange framework for FBX import
			"InterchangeCore",
			"InterchangeEngine",

			// Rendering (for thumbnail generation)
			"RenderCore",
			"RHI",

			// Level Sequencer
			"LevelSequence",
			"LevelSequenceEditor",
			"MovieScene",
			"MovieSceneTracks",
			"CinematicCamera",

			// Navigation / AI
			"NavigationSystem",
			"AIModule",

			// Landscape (engine module, always available)
			"Landscape",
			"LandscapeEditor",

			// Source Control (always available in editor)
			"SourceControl",

			// Derived Data Cache (engine-internal, always available) — powers
			// get_ddc_stats / purge_ddc / warm_ddc.
			"DerivedDataCache",

			// PCG - skipping the optional-dep gate here for now (only 1 command, and
			// "PCG" is a non-Optional plugin in the .uplugin). If you want PCG to be
			// truly optional, follow the WITH_ECA_NIAGARA pattern below.
			"PCG",

			// Enhanced Input - hard dep, EnhancedInput ships with the engine and is
			// non-Optional in the .uplugin.
			"EnhancedInput",

			// Python sandbox - requires PythonScriptPlugin (the "Python Editor Script Plugin").
			// Used by execute_script + get_execution_environment to invoke the Python
			// interpreter and expose a UFUNCTION-backed execute_tool() helper to scripts.
			"PythonScriptPlugin",

			// LiveLink interface - engine runtime module (NOT a plugin), always available.
			// The LiveLink plugin provides the modular-feature implementation; if it's
			// disabled, IsModularFeatureAvailable() returns false and the commands
			// short-circuit with a clear "plugin not enabled" message. No DelayLoad
			// needed because LiveLinkInterface ships in the engine itself.
			"LiveLinkInterface",

			// HeadMountedDisplay - engine module providing IXRTrackingSystem and
			// UHeadMountedDisplayFunctionLibrary. Hard-linked because the module is
			// always available; runtime probes (GEngine->XRSystem) determine whether
			// an actual HMD is connected. Used by the OpenXR introspection commands.
			"HeadMountedDisplay",

			// PhysicsCore - engine runtime module that owns UPhysicalMaterial,
			// EPhysicalSurface, EFrictionCombineMode reflection metadata. Needed for
			// the batch-G physics-inspection commands to link cleanly.
			"PhysicsCore"
		});

		// ModelViewViewModelBlueprint include path - UBT doesn't resolve it via PrivateDependencyModuleNames
		// when the plugin is EnabledByDefault=false, so add it explicitly.
		string MVVMBlueprintPath = System.IO.Path.Combine(EngineDirectory, "Plugins", "Runtime", "ModelViewViewModel", "Source", "ModelViewViewModelBlueprint", "Public");
		PrivateIncludePaths.Add(MVVMBlueprintPath);

		// ── Optional engine-plugin feature subsystems ──────────────────────
		// Each block conditionally links its DLLs only if the engine ships the plugin,
		// and exposes a WITH_ECA_<FEATURE> preprocessor symbol the source files use to
		// #if-gate the corresponding commands. A project that doesn't want these
		// subsystems just doesn't enable the upstream engine plugin — ECABridge will
		// load fine without them, with the relevant commands simply absent.

		// Each block below: gates compile-time deps + WITH_ECA_<FEATURE>, AND adds
		// PublicDelayLoadDLLs so the resulting binary loads even when the optional
		// plugin is disabled in the consumer's .uproject. ECABridgeModule::StartupModule
		// then probes each subsystem's primary module via FModuleManager::IsModuleLoaded
		// and unregisters that category's commands when missing — so the lazily-resolved
		// imports are never touched.

		// Mutable / Customizable Object (character customization, 13 commands)
		if (EngineHasPluginWithBinaries("Mutable"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "CustomizableObject", "CustomizableObjectEditor" });
			AddDelayLoadDLL("UnrealEditor-CustomizableObject.dll");
			AddDelayLoadDLL("UnrealEditor-CustomizableObjectEditor.dll");
			PublicDefinitions.Add("WITH_ECA_MUTABLE=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_MUTABLE=0");
		}

		// Movie Render Pipeline (high-quality offline rendering, 2 commands)
		if (EngineHasPluginWithBinaries("MovieRenderPipeline"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MovieRenderPipelineCore", "MovieRenderPipelineEditor", "MovieRenderPipelineRenderPasses" });
			AddDelayLoadDLL("UnrealEditor-MovieRenderPipelineCore.dll");
			AddDelayLoadDLL("UnrealEditor-MovieRenderPipelineEditor.dll");
			AddDelayLoadDLL("UnrealEditor-MovieRenderPipelineRenderPasses.dll");
			PublicDefinitions.Add("WITH_ECA_MOVIE_RENDER_PIPELINE=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_MOVIE_RENDER_PIPELINE=0");
		}

		// MetaHuman Character (procedural MetaHuman pipeline, 22 commands)
		if (EngineHasPluginWithBinaries("MetaHumanCharacter"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MetaHumanCharacter", "MetaHumanCharacterPalette" });
			AddDelayLoadDLL("UnrealEditor-MetaHumanCharacter.dll");
			AddDelayLoadDLL("UnrealEditor-MetaHumanCharacterPalette.dll");
			PublicDefinitions.Add("WITH_ECA_METAHUMAN_CHARACTER=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_METAHUMAN_CHARACTER=0");
		}

		// Niagara VFX (25 emitter/system/module commands + 2 NiagaraDataChannel commands).
		// NOTE: Niagara is NOT delay-loaded — its DLLs export data symbols like
		// FNiagaraTypeDefinition::FloatDef which the MSVC linker refuses to delay
		// (LNK1194). The IsModuleLoaded gate in StartupModule still drops Niagara
		// commands if the module happens to be missing, but a project that fully
		// disables Niagara would also fail at PE-load time. Treated as load-time
		// required when WITH_ECA_NIAGARA=1.
		if (EngineHasPlugin("Niagara"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "Niagara", "NiagaraCore", "NiagaraEditor", "NiagaraShader" });
			PublicDefinitions.Add("WITH_ECA_NIAGARA=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_NIAGARA=0");
		}

		// MetaSound procedural audio (16 commands). MetasoundFrontend cannot be
		// delay-loaded (exports static data symbol Metasound::FrontendInvalidID,
		// LNK1194). MetasoundEngine is the module we gate on at runtime, so if
		// either is absent the binary fails to load — the optionality story is
		// best-effort for this subsystem (the runtime registry gate still works
		// for "module unloaded after init").
		if (EngineHasPlugin("Metasound"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MetasoundEngine", "MetasoundEditor", "MetasoundFrontend", "MetasoundGraphCore", "MetasoundStandardNodes", "AudioExtensions" });
			AddDelayLoadDLL("UnrealEditor-MetasoundEditor.dll");
			AddDelayLoadDLL("UnrealEditor-MetasoundGraphCore.dll");
			AddDelayLoadDLL("UnrealEditor-MetasoundStandardNodes.dll");
			AddDelayLoadDLL("UnrealEditor-AudioExtensions.dll");
			PublicDefinitions.Add("WITH_ECA_METASOUND=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_METASOUND=0");
		}

		// Control Rig (animation rigging, 1 dump command)
		if (EngineHasPlugin("ControlRig"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "ControlRig", "ControlRigDeveloper" });
			AddDelayLoadDLL("UnrealEditor-ControlRig.dll");
			AddDelayLoadDLL("UnrealEditor-ControlRigDeveloper.dll");
			PublicDefinitions.Add("WITH_ECA_CONTROL_RIG=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_CONTROL_RIG=0");
		}

		// Gameplay Ability System (1 dump command)
		if (EngineHasPlugin("GameplayAbilities"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTags", "GameplayTasks" });
			AddDelayLoadDLL("UnrealEditor-GameplayAbilities.dll");
			AddDelayLoadDLL("UnrealEditor-GameplayTags.dll");
			AddDelayLoadDLL("UnrealEditor-GameplayTasks.dll");
			PublicDefinitions.Add("WITH_ECA_GAMEPLAY_ABILITIES=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_GAMEPLAY_ABILITIES=0");
		}

		// DMX (lighting protocol). DMXRuntime owns UDMXLibrary / UDMXEntityFixturePatch /
		// UDMXEntityFixtureType; DMXProtocol owns FDMXOutputPort + Send APIs used by
		// the runtime mutators (set_dmx_universe, send_dmx_values). Both link cleanly
		// under the same DMXEngine plugin gate.
		if (EngineHasPluginWithBinaries("DMXEngine"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "DMXRuntime", "DMXProtocol" });
			AddDelayLoadDLL("UnrealEditor-DMXRuntime.dll");
			AddDelayLoadDLL("UnrealEditor-DMXProtocol.dll");
			PublicDefinitions.Add("WITH_ECA_DMX=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_DMX=0");
		}

		// nDisplay (multi-display rendering / virtual production).
		// Uses EngineHasPluginWithBinaries — on Mac, nDisplay ships as engine source
		// but without compiled binaries in standard launcher installs.
		if (EngineHasPluginWithBinaries("nDisplay"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "DisplayCluster", "DisplayClusterConfiguration" });
			AddDelayLoadDLL("UnrealEditor-DisplayCluster.dll");
			AddDelayLoadDLL("UnrealEditor-DisplayClusterConfiguration.dll");
			PublicDefinitions.Add("WITH_ECA_NDISPLAY=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_NDISPLAY=0");
		}

		// USD Importer / Exporter (.usd / .usda / .usdc / .usdz I/O).
		// Optional: ships with the engine but disabled by default.
		if (EngineHasPluginWithBinaries("USDImporter"))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "USDStageImporter", "USDExporter" });
			AddDelayLoadDLL("UnrealEditor-USDStageImporter.dll");
			AddDelayLoadDLL("UnrealEditor-USDExporter.dll");
			PublicDefinitions.Add("WITH_ECA_USD=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_USD=0");
		}

		// DataValidation (stock editor plugin; powers validate_before_submit).
		if (EngineHasPluginWithBinaries("DataValidation"))
		{
			PrivateDependencyModuleNames.Add("DataValidation");
			AddDelayLoadDLL("UnrealEditor-DataValidation.dll");
			PublicDefinitions.Add("WITH_ECA_DATAVALIDATION=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_DATAVALIDATION=0");
		}

		// Native MCP coexistence bridge (Batch K). Compiled only when Epic's
		// ModelContextProtocol plugin ships with the engine (5.8+ Experimental).
		// We deliberately do NOT add a PrivateDependencyModuleNames entry for
		// "ModelContextProtocol" — it's NoRedist and we want zero link-time
		// coupling. Runtime detection uses FModuleManager::IsModuleLoaded.
		if (EngineHasPlugin("ModelContextProtocol"))
		{
			PublicDefinitions.Add("WITH_ECA_NATIVE_MCP_INTEGRATION=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_NATIVE_MCP_INTEGRATION=0");
		}
	}
}
