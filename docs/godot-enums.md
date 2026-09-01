# Godot の列挙体と、L^ に写すかどうか

`scripts/list-godot-enums.py` が生成する。手で書き換えない。

決めるための一覧である。02 の 19 章の `enum^` にするか、クラスの下の
定数にするか、そもそも写さないか。

- 宣言: **738**（うちグローバル 22、bitfield 34）
- 値の総数: **5247**
- **どこからも名指されない: 70**
- 宣言したクラスがバインドされていない: 164

## 印の読み方

| 印 | 意味 |
| --- | --- |
| **未使用** | 引数・返り値・プロパティ・シグナルのどこにも現れない。C++ の内部でだけ意味を持つ定数であり、GDScript の書き手にも用が無い |
| **到達不可** | 宣言したクラスを `godot-classes.txt` が選んでいない。写しても名前を書く相手が居ない |
| **bitfield** | フラグの集合。OR で組むものなので `enum^` には合わない |
| **editor** | エディター API。配布ビルドには無い |

`引数` / `返り` は**バインド済みで呼べるメソッド**での回数。括弧内はエンジン API 全体での回数（virtual・vararg・static を含む）。

## グローバル — 使われているもの

モジュールのもの。クラスに属さないので、写すならそのまま `godot.名前`。

| 列挙体 | 値 | 引数 | 返り | プロパティ | シグナル | 印 |
| --- | --- | --- | --- | --- | --- | --- |
| `ClockDirection` | 2 | 1 | 0 | 0 | 0 | — |
| `Corner` | 4 | 2 | 0 | 0 | 0 | — |
| `Error` | 49 | 0 | 196 (261) | 0 | 0 | — |
| `EulerOrder` | 6 | 1 | 1 | 0 | 0 | — |
| `HorizontalAlignment` | 4 | 26 | 13 | 0 | 0 | — |
| `InlineAlignment` | 13 | 9 (11) | 0 | 0 | 0 | — |
| `JoyAxis` | 9 | 2 | 1 | 0 | 0 | — |
| `JoyButton` | 24 | 2 | 1 | 0 | 0 | — |
| `Key` | 193 | 33 | 12 | 0 | 0 | — |
| `KeyLocation` | 3 | 1 | 1 | 0 | 0 | — |
| `KeyModifierMask` | 9 | 0 | 1 | 0 | 0 | bitfield |
| `MIDIMessage` | 19 | 1 | 1 | 0 | 0 | — |
| `MouseButton` | 10 | 2 | 1 | 0 | 0 | — |
| `MouseButtonMask` | 5 | 2 | 4 | 0 | 0 | bitfield |
| `PropertyHint` | 45 | 2 (4) | 0 | 0 | 0 | — |
| `PropertyUsageFlags` | 32 | 0 (1) | 0 | 0 | 0 | bitfield |
| `Side` | 4 | 27 | 0 | 0 | 0 | — |
| `Variant.Type` | 40 | 5 (7) | 2 | 0 | 0 | — |
| `VerticalAlignment` | 4 | 5 | 5 | 0 | 0 | — |

## クラス — 使われているもの

クラスの下にあるもの。同じ綴りが別のクラスにもあるので、`enum^` にするなら名前を一意にする必要がある。

| 列挙体 | 値 | 引数 | 返り | プロパティ | シグナル | 印 |
| --- | --- | --- | --- | --- | --- | --- |
| `AESContext.Mode` | 5 | 1 | 0 | 0 | 0 | — |
| `AStarGrid2D.CellShape` | 4 | 1 | 1 | 0 | 0 | — |
| `AStarGrid2D.DiagonalMode` | 5 | 1 | 1 | 0 | 0 | — |
| `AStarGrid2D.Heuristic` | 5 | 2 | 2 | 0 | 0 | — |
| `Animation.FindMode` | 3 | 1 | 0 | 0 | 0 | — |
| `Animation.InterpolationType` | 5 | 1 | 1 | 0 | 0 | — |
| `Animation.LoopMode` | 3 | 2 | 2 | 0 | 0 | — |
| `Animation.LoopedFlag` | 3 | 1 | 0 | 0 | 0 | — |
| `Animation.TrackType` | 9 | 2 | 1 | 0 | 0 | — |
| `Animation.UpdateMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationMixer.AnimationCallbackModeDiscrete` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationMixer.AnimationCallbackModeMethod` | 2 | 1 | 1 | 0 | 0 | — |
| `AnimationMixer.AnimationCallbackModeProcess` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationNode.FilterAction` | 4 | 2 | 0 | 0 | 0 | — |
| `AnimationNodeAnimation.PlayMode` | 2 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeBlendSpace1D.BlendMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeBlendSpace2D.BlendMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeOneShot.MixMode` | 2 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeStateMachine.StateMachineType` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeStateMachineTransition.AdvanceMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationNodeStateMachineTransition.SwitchMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationPlayer.AnimationMethodCallMode` | 2 | 1 | 1 | 0 | 0 | — |
| `AnimationPlayer.AnimationProcessCallback` | 3 | 1 | 1 | 0 | 0 | — |
| `AnimationTree.AnimationProcessCallback` | 3 | 1 | 1 | 0 | 0 | — |
| `Area2D.SpaceOverride` | 5 | 3 | 3 | 0 | 0 | — |
| `Area3D.SpaceOverride` | 5 | 3 | 3 | 0 | 0 | — |
| `AspectRatioContainer.AlignmentMode` | 3 | 2 | 2 | 0 | 0 | — |
| `AspectRatioContainer.StretchMode` | 4 | 1 | 1 | 0 | 0 | — |
| `AudioEffectDistortion.Mode` | 5 | 1 | 1 | 0 | 0 | — |
| `AudioEffectFilter.FilterDB` | 4 | 1 | 1 | 0 | 0 | — |
| `AudioEffectPitchShift.FFTSize` | 6 | 1 | 1 | 0 | 0 | — |
| `AudioEffectSpectrumAnalyzer.FFTSize` | 6 | 1 | 1 | 0 | 0 | — |
| `AudioEffectSpectrumAnalyzerInstance.MagnitudeMode` | 2 | 1 | 0 | 0 | 0 | — |
| `AudioListener3D.DopplerTracking` | 3 | 1 | 1 | 0 | 0 | — |
| `AudioServer.PlaybackType` | 4 | 4 | 3 | 0 | 0 | **到達不可** |
| `AudioServer.SpeakerMode` | 4 | 0 | 1 (2) | 0 | 0 | **到達不可** |
| `AudioStreamGenerator.AudioStreamGeneratorMixRate` | 4 | 1 | 1 | 0 | 0 | — |
| `AudioStreamInteractive.AutoAdvanceMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AudioStreamInteractive.FadeMode` | 5 | 1 | 1 | 0 | 0 | — |
| `AudioStreamInteractive.TransitionFromTime` | 4 | 1 | 1 | 0 | 0 | — |
| `AudioStreamInteractive.TransitionToTime` | 2 | 1 | 1 | 0 | 0 | — |
| `AudioStreamPlayer.MixTarget` | 3 | 1 | 1 | 0 | 0 | — |
| `AudioStreamPlayer3D.AttenuationModel` | 4 | 1 | 1 | 0 | 0 | — |
| `AudioStreamPlayer3D.DopplerTracking` | 3 | 1 | 1 | 0 | 0 | — |
| `AudioStreamRandomizer.PlaybackMode` | 3 | 1 | 1 | 0 | 0 | — |
| `AudioStreamWAV.Format` | 4 | 2 | 2 | 0 | 0 | — |
| `AudioStreamWAV.LoopMode` | 4 | 1 | 1 | 0 | 0 | — |
| `BackBufferCopy.CopyMode` | 3 | 1 | 1 | 0 | 0 | — |
| `BaseButton.ActionMode` | 2 | 1 | 1 | 0 | 0 | — |
| `BaseButton.DrawMode` | 5 | 0 | 1 | 0 | 0 | — |
| `BaseMaterial3D.AlphaAntiAliasing` | 3 | 3 | 3 | 0 | 0 | — |
| `BaseMaterial3D.BillboardMode` | 4 | 3 | 3 | 0 | 0 | — |
| `BaseMaterial3D.BlendMode` | 5 | 2 | 2 | 0 | 0 | — |
| `BaseMaterial3D.CullMode` | 3 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.DepthDrawMode` | 3 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.DepthTest` | 2 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.DetailUV` | 2 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.DiffuseMode` | 4 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.DistanceFadeMode` | 4 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.EmissionOperator` | 2 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.Feature` | 14 | 2 | 0 | 0 | 0 | — |
| `BaseMaterial3D.Flags` | 26 | 2 | 0 | 0 | 0 | — |
| `BaseMaterial3D.ShadingMode` | 4 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.SpecularMode` | 3 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.StencilCompare` | 7 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.StencilMode` | 4 | 1 | 1 | 0 | 0 | — |
| `BaseMaterial3D.TextureChannel` | 5 | 4 | 4 | 0 | 0 | — |
| `BaseMaterial3D.TextureFilter` | 7 | 3 | 3 | 0 | 0 | — |
| `BaseMaterial3D.TextureParam` | 20 | 2 | 0 | 0 | 0 | — |
| `BaseMaterial3D.Transparency` | 6 | 1 | 1 | 0 | 0 | — |
| `BoxContainer.AlignmentMode` | 3 | 1 | 1 | 0 | 0 | — |
| `CPUParticles2D.DrawOrder` | 2 | 1 | 1 | 0 | 0 | — |
| `CPUParticles2D.EmissionShape` | 7 | 1 | 1 | 0 | 0 | — |
| `CPUParticles2D.Parameter` | 13 | 6 | 0 | 0 | 0 | — |
| `CPUParticles2D.ParticleFlags` | 4 | 2 | 0 | 0 | 0 | — |
| `CPUParticles3D.DrawOrder` | 3 | 1 | 1 | 0 | 0 | — |
| `CPUParticles3D.EmissionShape` | 8 | 1 | 1 | 0 | 0 | — |
| `CPUParticles3D.Parameter` | 13 | 6 | 0 | 0 | 0 | — |
| `CPUParticles3D.ParticleFlags` | 4 | 2 | 0 | 0 | 0 | — |
| `CSGPolygon3D.Mode` | 3 | 1 | 1 | 0 | 0 | — |
| `CSGPolygon3D.PathIntervalType` | 2 | 1 | 1 | 0 | 0 | — |
| `CSGPolygon3D.PathRotation` | 3 | 1 | 1 | 0 | 0 | — |
| `CSGShape3D.Operation` | 3 | 1 | 1 | 0 | 0 | — |
| `Camera2D.AnchorMode` | 2 | 1 | 1 | 0 | 0 | — |
| `Camera2D.Camera2DProcessCallback` | 2 | 1 | 1 | 0 | 0 | — |
| `Camera3D.DopplerTracking` | 3 | 1 | 1 | 0 | 0 | — |
| `Camera3D.KeepAspect` | 2 | 1 | 1 | 0 | 0 | — |
| `Camera3D.ProjectionType` | 3 | 1 | 1 | 0 | 0 | — |
| `CameraFeed.FeedDataType` | 5 | 0 | 1 | 0 | 0 | — |
| `CameraFeed.FeedPosition` | 3 | 1 | 1 | 0 | 0 | — |
| `CameraServer.FeedImage` | 4 | 2 | 1 | 0 | 0 | **到達不可** |
| `CanvasItem.ClipChildrenMode` | 4 | 1 | 1 | 0 | 0 | — |
| `CanvasItem.TextureFilter` | 8 | 2 | 2 | 0 | 0 | — |
| `CanvasItem.TextureRepeat` | 5 | 2 | 2 | 0 | 0 | — |
| `CanvasItemMaterial.BlendMode` | 5 | 1 | 1 | 0 | 0 | — |
| `CanvasItemMaterial.LightMode` | 3 | 1 | 1 | 0 | 0 | — |
| `CharacterBody2D.MotionMode` | 2 | 1 | 1 | 0 | 0 | — |
| `CharacterBody2D.PlatformOnLeave` | 3 | 1 | 1 | 0 | 0 | — |
| `CharacterBody3D.MotionMode` | 2 | 1 | 1 | 0 | 0 | — |
| `CharacterBody3D.PlatformOnLeave` | 3 | 1 | 1 | 0 | 0 | — |
| `ClassDB.APIType` | 5 | 0 | 1 | 0 | 0 | **到達不可** |
| `CodeEdit.CodeCompletionKind` | 10 | 1 | 0 | 0 | 0 | — |
| `CollisionObject2D.DisableMode` | 3 | 1 | 1 | 0 | 0 | — |
| `CollisionObject3D.DisableMode` | 3 | 1 | 1 | 0 | 0 | — |
| `CollisionPolygon2D.BuildMode` | 2 | 1 | 1 | 0 | 0 | — |
| `ColorPicker.ColorModeType` | 5 | 1 | 1 | 0 | 0 | — |
| `ColorPicker.PickerShapeType` | 7 | 1 | 1 | 0 | 0 | — |
| `CompositorEffect.EffectCallbackType` | 6 | 1 | 1 | 0 | 0 | — |
| `ConeTwistJoint3D.Param` | 6 | 2 | 0 | 0 | 0 | — |
| `Control.CursorShape` | 17 | 1 | 2 | 0 | 0 | — |
| `Control.FocusBehaviorRecursive` | 3 | 1 | 1 | 0 | 0 | — |
| `Control.FocusMode` | 4 | 3 | 4 | 0 | 0 | — |
| `Control.GrowDirection` | 3 | 2 | 2 | 0 | 0 | — |
| `Control.LayoutDirection` | 7 | 1 | 1 | 0 | 0 | — |
| `Control.LayoutPreset` | 16 | 3 | 0 | 0 | 0 | — |
| `Control.LayoutPresetMode` | 4 | 2 | 0 | 0 | 0 | — |
| `Control.MouseBehaviorRecursive` | 3 | 1 | 1 | 0 | 0 | — |
| `Control.MouseFilter` | 3 | 1 | 2 | 0 | 0 | — |
| `Control.SizeFlags` | 6 | 2 | 2 | 0 | 0 | bitfield |
| `Control.TextDirection` | 4 | 14 | 13 | 0 | 0 | — |
| `ConvertTransformModifier3D.TransformMode` | 3 | 2 | 2 | 0 | 0 | — |
| `CopyTransformModifier3D.AxisFlag` | 4 | 2 | 2 | 0 | 0 | bitfield |
| `CopyTransformModifier3D.TransformFlag` | 4 | 1 | 1 | 0 | 0 | bitfield |
| `Curve.TangentMode` | 3 | 4 | 2 | 0 | 0 | — |
| `CurveTexture.TextureMode` | 2 | 1 | 1 | 0 | 0 | — |
| `Decal.DecalTexture` | 5 | 2 | 0 | 0 | 0 | — |
| `DirectionalLight3D.ShadowMode` | 3 | 1 | 1 | 0 | 0 | — |
| `DirectionalLight3D.SkyMode` | 3 | 1 | 1 | 0 | 0 | — |
| `DisplayServer.AccessibilityAction` | 23 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.AccessibilityFlags` | 10 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.AccessibilityLiveMode` | 3 | 2 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.AccessibilityPopupType` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.AccessibilityRole` | 46 | 3 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.CursorShape` | 18 | 2 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.Feature` | 34 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.FileDialogMode` | 5 | 2 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.HandleType` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.MouseMode` | 6 | 1 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.ScreenOrientation` | 7 | 1 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.TTSUtteranceEvent` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.VSyncMode` | 4 | 1 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.VirtualKeyboardType` | 8 | 1 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.WindowFlags` | 14 | 2 | 0 | 0 | 0 | **到達不可** |
| `DisplayServer.WindowMode` | 5 | 1 | 1 | 0 | 0 | **到達不可** |
| `DisplayServer.WindowResizeEdge` | 9 | 2 | 0 | 0 | 0 | **到達不可** |
| `ENetConnection.CompressionMode` | 5 | 1 | 0 | 0 | 0 | — |
| `ENetConnection.HostStatistic` | 4 | 1 | 0 | 0 | 0 | — |
| `ENetPacketPeer.PeerState` | 10 | 0 | 1 | 0 | 0 | — |
| `ENetPacketPeer.PeerStatistic` | 14 | 1 | 0 | 0 | 0 | — |
| `EditorContextMenuPlugin.ContextMenuSlot` | 7 | 1 | 0 | 0 | 0 | editor |
| `EditorExportPlatform.DebugFlags` | 5 | 6 (12) | 0 | 0 | 0 | bitfield editor |
| `EditorExportPlatform.ExportMessageType` | 4 | 1 | 2 | 0 | 0 | editor |
| `EditorExportPreset.ExportFilter` | 5 | 0 | 1 | 0 | 0 | editor |
| `EditorExportPreset.FileExportMode` | 4 | 1 | 1 | 0 | 0 | editor |
| `EditorFeatureProfile.Feature` | 10 | 3 | 0 | 0 | 0 | editor |
| `EditorFileDialog.Access` | 3 | 1 | 1 | 0 | 0 | editor |
| `EditorFileDialog.DisplayMode` | 2 | 1 | 1 | 0 | 0 | editor |
| `EditorFileDialog.FileMode` | 5 | 1 | 1 | 0 | 0 | editor |
| `EditorPlugin.CustomControlContainer` | 12 | 2 | 0 | 0 | 0 | editor |
| `EditorPlugin.DockSlot` | 9 | 1 | 0 | 0 | 0 | editor |
| `EditorToaster.Severity` | 3 | 1 | 0 | 0 | 0 | editor |
| `EditorVCSInterface.ChangeType` | 6 | 1 | 0 | 0 | 0 | editor |
| `EditorVCSInterface.TreeArea` | 3 | 1 | 0 | 0 | 0 | editor |
| `Environment.AmbientSource` | 4 | 1 | 1 | 0 | 0 | — |
| `Environment.BGMode` | 7 | 1 | 1 | 0 | 0 | — |
| `Environment.FogMode` | 2 | 1 | 1 | 0 | 0 | — |
| `Environment.GlowBlendMode` | 5 | 1 | 1 | 0 | 0 | — |
| `Environment.ReflectionSource` | 3 | 1 | 1 | 0 | 0 | — |
| `Environment.SDFGIYScale` | 3 | 1 | 1 | 0 | 0 | — |
| `Environment.ToneMapper` | 5 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.CellularDistanceFunction` | 4 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.CellularReturnType` | 7 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.DomainWarpFractalType` | 3 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.DomainWarpType` | 3 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.FractalType` | 4 | 1 | 1 | 0 | 0 | — |
| `FastNoiseLite.NoiseType` | 6 | 1 | 1 | 0 | 0 | — |
| `FileAccess.CompressionMode` | 5 | 0 (1) | 0 | 0 | 0 | — |
| `FileAccess.ModeFlags` | 4 | 0 (4) | 0 | 0 | 0 | — |
| `FileAccess.UnixPermissionFlags` | 12 | 0 (1) | 0 (1) | 0 | 0 | bitfield |
| `FileDialog.Access` | 3 | 1 | 1 | 0 | 0 | — |
| `FileDialog.Customization` | 7 | 2 | 0 | 0 | 0 | — |
| `FileDialog.DisplayMode` | 2 | 1 | 1 | 0 | 0 | — |
| `FileDialog.FileMode` | 5 | 1 | 1 | 0 | 0 | — |
| `FlowContainer.AlignmentMode` | 3 | 1 | 1 | 0 | 0 | — |
| `FlowContainer.LastWrapAlignmentMode` | 4 | 1 | 1 | 0 | 0 | — |
| `FoldableContainer.TitlePosition` | 2 | 1 | 1 | 0 | 0 | — |
| `GDExtension.InitializationLevel` | 4 | 0 | 1 | 0 | 0 | — |
| `GDExtensionManager.LoadStatus` | 5 | 0 | 3 | 0 | 0 | **到達不可** |
| `GLTFAccessor.GLTFAccessorType` | 7 | 1 | 2 | 0 | 0 | — |
| `GLTFAccessor.GLTFComponentType` | 12 | 2 | 2 | 0 | 0 | — |
| `GLTFDocument.RootNodeMode` | 3 | 1 | 1 | 0 | 0 | — |
| `GLTFDocument.VisibilityMode` | 3 | 1 | 1 | 0 | 0 | — |
| `GLTFObjectModelProperty.GLTFObjectModelType` | 11 | 2 | 1 | 0 | 0 | — |
| `GPUParticles2D.DrawOrder` | 3 | 1 | 1 | 0 | 0 | — |
| `GPUParticles3D.DrawOrder` | 4 | 1 | 1 | 0 | 0 | — |
| `GPUParticles3D.TransformAlign` | 4 | 1 | 1 | 0 | 0 | — |
| `GPUParticlesCollisionHeightField3D.Resolution` | 7 | 1 | 1 | 0 | 0 | — |
| `GPUParticlesCollisionHeightField3D.UpdateMode` | 2 | 1 | 1 | 0 | 0 | — |
| `GPUParticlesCollisionSDF3D.Resolution` | 7 | 1 | 1 | 0 | 0 | — |
| `Generic6DOFJoint3D.Flag` | 7 | 6 | 0 | 0 | 0 | — |
| `Generic6DOFJoint3D.Param` | 23 | 6 | 0 | 0 | 0 | — |
| `Geometry2D.PolyEndType` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `Geometry2D.PolyJoinType` | 3 | 2 | 0 | 0 | 0 | **到達不可** |
| `GeometryInstance3D.GIMode` | 3 | 1 | 1 | 0 | 0 | — |
| `GeometryInstance3D.LightmapScale` | 5 | 1 | 1 | 0 | 0 | — |
| `GeometryInstance3D.ShadowCastingSetting` | 4 | 2 | 2 | 0 | 0 | — |
| `GeometryInstance3D.VisibilityRangeFadeMode` | 3 | 2 | 2 | 0 | 0 | — |
| `Gradient.ColorSpace` | 3 | 1 | 1 | 0 | 0 | — |
| `Gradient.InterpolationMode` | 3 | 1 | 1 | 0 | 0 | — |
| `GradientTexture2D.Fill` | 3 | 1 | 1 | 0 | 0 | — |
| `GradientTexture2D.Repeat` | 3 | 1 | 1 | 0 | 0 | — |
| `GraphEdit.GridPattern` | 2 | 1 | 1 | 0 | 0 | — |
| `GraphEdit.PanningScheme` | 2 | 1 | 1 | 0 | 0 | — |
| `HTTPClient.Method` | 10 | 4 | 0 | 0 | 0 | — |
| `HTTPClient.Status` | 10 | 0 | 2 | 0 | 0 | — |
| `HashingContext.HashType` | 3 | 5 | 0 | 0 | 0 | — |
| `HingeJoint3D.Flag` | 3 | 2 | 0 | 0 | 0 | — |
| `HingeJoint3D.Param` | 9 | 2 | 0 | 0 | 0 | — |
| `IP.ResolverStatus` | 4 | 0 | 1 | 0 | 0 | **到達不可** |
| `IP.Type` | 4 | 3 | 0 | 0 | 0 | **到達不可** |
| `Image.ASTCFormat` | 2 | 2 | 0 | 0 | 0 | — |
| `Image.AlphaMode` | 3 | 0 | 1 | 0 | 0 | — |
| `Image.CompressMode` | 6 | 2 | 0 | 0 | 0 | — |
| `Image.CompressSource` | 3 | 2 | 0 | 0 | 0 | — |
| `Image.Format` | 40 | 5 (8) | 6 (8) | 0 | 0 | — |
| `Image.Interpolation` | 5 | 2 | 0 | 0 | 0 | — |
| `Image.UsedChannels` | 6 | 1 | 1 | 0 | 0 | — |
| `ImageFormatLoader.LoaderFlags` | 3 | 0 (1) | 0 | 0 | 0 | bitfield |
| `Input.CursorShape` | 17 | 2 | 1 | 0 | 0 | **到達不可** |
| `Input.MouseMode` | 6 | 1 | 1 | 0 | 0 | **到達不可** |
| `ItemList.IconMode` | 2 | 1 | 1 | 0 | 0 | — |
| `ItemList.SelectMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Label3D.AlphaCutMode` | 4 | 1 | 1 | 0 | 0 | — |
| `Label3D.DrawFlags` | 5 | 2 | 0 | 0 | 0 | — |
| `Light2D.BlendMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Light2D.ShadowFilter` | 3 | 1 | 1 | 0 | 0 | — |
| `Light3D.BakeMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Light3D.Param` | 22 | 2 | 0 | 0 | 0 | — |
| `LightmapGI.BakeQuality` | 4 | 1 | 1 | 0 | 0 | — |
| `LightmapGI.EnvironmentMode` | 4 | 1 | 1 | 0 | 0 | — |
| `LightmapGI.GenerateProbes` | 5 | 1 | 1 | 0 | 0 | — |
| `LightmapGIData.ShadowmaskMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Line2D.LineCapMode` | 3 | 2 | 2 | 0 | 0 | — |
| `Line2D.LineJointMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Line2D.LineTextureMode` | 3 | 1 | 1 | 0 | 0 | — |
| `LineEdit.VirtualKeyboardType` | 8 | 1 | 1 | 0 | 0 | — |
| `LinkButton.UnderlineMode` | 3 | 1 | 1 | 0 | 0 | — |
| `LookAtModifier3D.OriginFrom` | 3 | 1 | 1 | 0 | 0 | — |
| `Mesh.ArrayFormat` | 27 | 1 | 1 | 0 | 0 | bitfield |
| `Mesh.BlendShapeMode` | 2 | 2 | 2 | 0 | 0 | — |
| `Mesh.PrimitiveType` | 5 | 5 | 3 | 0 | 0 | — |
| `MeshConvexDecompositionSettings.Mode` | 2 | 1 | 1 | 0 | 0 | — |
| `MultiMesh.PhysicsInterpolationQuality` | 2 | 1 | 1 | 0 | 0 | — |
| `MultiMesh.TransformFormat` | 2 | 1 | 1 | 0 | 0 | — |
| `MultiplayerPeer.ConnectionStatus` | 3 | 0 | 1 (2) | 0 | 0 | — |
| `MultiplayerPeer.TransferMode` | 3 | 2 (3) | 2 (4) | 0 | 0 | — |
| `MultiplayerSynchronizer.VisibilityUpdateMode` | 3 | 1 | 1 | 0 | 0 | — |
| `NativeMenu.Feature` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `NativeMenu.SystemMenus` | 6 | 4 | 1 | 0 | 0 | **到達不可** |
| `NavigationMesh.ParsedGeometryType` | 4 | 1 | 1 | 0 | 0 | — |
| `NavigationMesh.SamplePartitionType` | 4 | 1 | 1 | 0 | 0 | — |
| `NavigationMesh.SourceGeometryMode` | 4 | 1 | 1 | 0 | 0 | — |
| `NavigationPathQueryParameters2D.PathMetadataFlags` | 5 | 2 | 2 | 0 | 0 | bitfield |
| `NavigationPathQueryParameters2D.PathPostProcessing` | 3 | 2 | 2 | 0 | 0 | — |
| `NavigationPathQueryParameters2D.PathfindingAlgorithm` | 1 | 2 | 2 | 0 | 0 | — |
| `NavigationPathQueryParameters3D.PathMetadataFlags` | 5 | 2 | 2 | 0 | 0 | bitfield |
| `NavigationPathQueryParameters3D.PathPostProcessing` | 3 | 2 | 2 | 0 | 0 | — |
| `NavigationPathQueryParameters3D.PathfindingAlgorithm` | 1 | 2 | 2 | 0 | 0 | — |
| `NavigationPolygon.ParsedGeometryType` | 4 | 1 | 1 | 0 | 0 | — |
| `NavigationPolygon.SamplePartitionType` | 3 | 1 | 1 | 0 | 0 | — |
| `NavigationPolygon.SourceGeometryMode` | 4 | 1 | 1 | 0 | 0 | — |
| `NavigationServer2D.ProcessInfo` | 10 | 1 | 0 | 0 | 0 | **到達不可** |
| `NavigationServer3D.ProcessInfo` | 10 | 1 | 0 | 0 | 0 | **到達不可** |
| `NinePatchRect.AxisStretchMode` | 3 | 2 | 2 | 0 | 0 | — |
| `Node.AutoTranslateMode` | 3 | 6 | 6 | 0 | 0 | — |
| `Node.InternalMode` | 3 | 1 | 0 | 0 | 0 | — |
| `Node.PhysicsInterpolationMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Node.ProcessMode` | 5 | 1 | 1 | 0 | 0 | — |
| `Node.ProcessThreadGroup` | 3 | 1 | 1 | 0 | 0 | — |
| `Node.ProcessThreadMessages` | 3 | 1 | 1 | 0 | 0 | bitfield |
| `Node3D.RotationEditMode` | 3 | 1 | 1 | 0 | 0 | — |
| `OS.StdHandleType` | 5 | 0 | 3 | 0 | 0 | **到達不可** |
| `OS.SystemDir` | 8 | 1 | 0 | 0 | 0 | **到達不可** |
| `OccluderPolygon2D.CullMode` | 3 | 1 | 1 | 0 | 0 | — |
| `OmniLight3D.ShadowMode` | 2 | 1 | 1 | 0 | 0 | — |
| `OpenXRAPIExtension.OpenXRAlphaBlendModeSupport` | 3 | 0 | 1 | 0 | 0 | — |
| `OpenXRAction.ActionType` | 4 | 2 | 1 | 0 | 0 | — |
| `OpenXRCompositionLayer.Filter` | 3 | 2 | 2 | 0 | 0 | — |
| `OpenXRCompositionLayer.MipmapMode` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRCompositionLayer.Swizzle` | 6 | 4 | 4 | 0 | 0 | — |
| `OpenXRCompositionLayer.Wrap` | 5 | 2 | 2 | 0 | 0 | — |
| `OpenXRFutureResult.ResultStatus` | 3 | 0 | 1 | 0 | 0 | — |
| `OpenXRHand.BoneUpdate` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRHand.Hands` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRHand.MotionRange` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRHand.SkeletonRig` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRInterface.Hand` | 3 | 9 | 0 | 0 | 0 | — |
| `OpenXRInterface.HandJointFlags` | 7 | 0 | 1 | 0 | 0 | bitfield |
| `OpenXRInterface.HandJoints` | 27 | 6 | 0 | 0 | 0 | — |
| `OpenXRInterface.HandMotionRange` | 3 | 1 | 1 | 0 | 0 | — |
| `OpenXRInterface.HandTrackedSource` | 4 | 0 | 1 | 0 | 0 | — |
| `OpenXRInterface.PerfSettingsLevel` | 4 | 2 | 0 | 0 | 0 | — |
| `OpenXRInterface.SessionState` | 9 | 0 | 1 | 0 | 0 | — |
| `OpenXRRenderModelManager.RenderModelTracker` | 4 | 1 | 1 | 0 | 0 | — |
| `PackedScene.GenEditState` | 4 | 1 | 0 | 0 | 0 | — |
| `PacketPeerDTLS.Status` | 5 | 0 | 1 | 0 | 0 | — |
| `ParticleProcessMaterial.CollisionMode` | 4 | 1 | 1 | 0 | 0 | — |
| `ParticleProcessMaterial.EmissionShape` | 8 | 1 | 1 | 0 | 0 | — |
| `ParticleProcessMaterial.Parameter` | 19 | 8 | 0 | 0 | 0 | — |
| `ParticleProcessMaterial.ParticleFlags` | 5 | 2 | 0 | 0 | 0 | — |
| `ParticleProcessMaterial.SubEmitterMode` | 6 | 1 | 1 | 0 | 0 | — |
| `PathFollow3D.RotationMode` | 5 | 1 (2) | 1 | 0 | 0 | — |
| `Performance.Monitor` | 60 | 1 | 0 | 0 | 0 | **到達不可** |
| `PhysicalBone3D.DampMode` | 2 | 2 | 2 | 0 | 0 | — |
| `PhysicalBone3D.JointType` | 6 | 1 | 1 | 0 | 0 | — |
| `PhysicsServer2D.AreaParameter` | 10 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.BodyMode` | 4 | 1 (2) | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer2D.BodyParameter` | 11 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.BodyState` | 5 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.CCDMode` | 3 | 1 (2) | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer2D.DampedSpringParam` | 3 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.JointParam` | 3 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.JointType` | 4 | 0 | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer2D.PinJointFlag` | 2 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.PinJointParam` | 4 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.ProcessInfo` | 3 | 1 (2) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer2D.ShapeType` | 9 | 0 | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer2D.SpaceParameter` | 9 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.AreaParameter` | 14 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.BodyAxis` | 6 | 4 (6) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.BodyMode` | 4 | 1 (2) | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer3D.BodyParameter` | 11 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.BodyState` | 5 | 4 (8) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.ConeTwistJointParam` | 5 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.G6DOFJointAxisFlag` | 7 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.G6DOFJointAxisParam` | 23 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.HingeJointFlag` | 2 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.HingeJointParam` | 8 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.JointType` | 6 | 0 | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer3D.PinJointParam` | 3 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.ProcessInfo` | 3 | 1 (2) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.ShapeType` | 11 | 0 | 1 (2) | 0 | 0 | **到達不可** |
| `PhysicsServer3D.SliderJointParam` | 23 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PhysicsServer3D.SpaceParameter` | 8 | 2 (4) | 0 | 0 | 0 | **到達不可** |
| `PinJoint3D.Param` | 3 | 2 | 0 | 0 | 0 | — |
| `PlaneMesh.Orientation` | 3 | 1 | 1 | 0 | 0 | — |
| `PortableCompressedTexture2D.CompressionMode` | 7 | 1 | 1 | 0 | 0 | — |
| `ReflectionProbe.AmbientMode` | 3 | 1 | 1 | 0 | 0 | — |
| `ReflectionProbe.UpdateMode` | 2 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.BarrierMask` | 7 | 2 | 0 | 0 | 0 | bitfield |
| `RenderingDevice.BlendFactor` | 20 | 4 | 4 | 0 | 0 | — |
| `RenderingDevice.BlendOperation` | 6 | 2 | 2 | 0 | 0 | — |
| `RenderingDevice.BufferCreationBits` | 2 | 4 | 0 | 0 | 0 | bitfield |
| `RenderingDevice.CompareOperator` | 9 | 4 | 4 | 0 | 0 | — |
| `RenderingDevice.DataFormat` | 233 | 11 | 4 | 0 | 0 | — |
| `RenderingDevice.DeviceType` | 6 | 0 | 1 | 0 | 0 | — |
| `RenderingDevice.DrawFlags` | 27 | 1 | 0 | 0 | 0 | bitfield |
| `RenderingDevice.DriverResource` | 26 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.Features` | 4 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.FinalAction` | 5 | 2 | 0 | 0 | 0 | — |
| `RenderingDevice.IndexBufferFormat` | 2 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.InitialAction` | 9 | 2 | 0 | 0 | 0 | — |
| `RenderingDevice.Limit` | 39 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.LogicOperation` | 17 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.MemoryType` | 3 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.PipelineDynamicStateFlags` | 7 | 1 | 0 | 0 | 0 | bitfield |
| `RenderingDevice.PolygonCullMode` | 3 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.PolygonFrontFace` | 2 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.RenderPrimitive` | 12 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.SamplerBorderColor` | 7 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.SamplerFilter` | 2 | 4 | 3 | 0 | 0 | — |
| `RenderingDevice.SamplerRepeatMode` | 6 | 3 | 3 | 0 | 0 | — |
| `RenderingDevice.ShaderLanguage` | 2 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.ShaderStage` | 11 | 6 | 0 | 0 | 0 | — |
| `RenderingDevice.StencilOperation` | 9 | 6 | 6 | 0 | 0 | — |
| `RenderingDevice.StorageBufferUsage` | 1 | 1 | 0 | 0 | 0 | bitfield |
| `RenderingDevice.TextureSamples` | 8 | 7 | 5 | 0 | 0 | — |
| `RenderingDevice.TextureSliceType` | 3 | 1 | 0 | 0 | 0 | — |
| `RenderingDevice.TextureSwizzle` | 8 | 4 | 4 | 0 | 0 | — |
| `RenderingDevice.TextureType` | 8 | 2 | 1 | 0 | 0 | — |
| `RenderingDevice.TextureUsageBits` | 10 | 3 | 1 | 0 | 0 | bitfield |
| `RenderingDevice.UniformType` | 11 | 1 | 1 | 0 | 0 | — |
| `RenderingDevice.VertexFrequency` | 2 | 1 | 1 | 0 | 0 | — |
| `RenderingServer.ArrayFormat` | 33 | 7 | 0 | 0 | 0 | bitfield **到達不可** |
| `RenderingServer.BlendShapeMode` | 2 | 1 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasGroupMode` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasItemTextureFilter` | 8 | 3 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasItemTextureRepeat` | 5 | 3 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasLightBlendMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasLightMode` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasLightShadowFilter` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasOccluderPolygonCullMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CanvasTextureChannel` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CompositorEffectCallbackType` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.CompositorEffectFlags` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.DOFBlurQuality` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.DOFBokehShape` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.DecalFilter` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.DecalTexture` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentAmbientSource` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentBG` | 7 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentFogMode` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentGlowBlendMode` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentReflectionSource` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSDFGIFramesToConverge` | 7 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSDFGIFramesToUpdateLight` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSDFGIRayCount` | 8 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSDFGIYScale` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSSAOQuality` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSSILQuality` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentSSRRoughnessQuality` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.EnvironmentToneMapper` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.Features` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.FogVolumeShape` | 6 | 2 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.GlobalShaderParameterType` | 30 | 1 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.InstanceFlags` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightBakeMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightDirectionalShadowMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightDirectionalSkyMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightOmniShadowMode` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightParam` | 22 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.LightProjectorFilter` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.MultimeshPhysicsInterpolationQuality` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.MultimeshTransformFormat` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.NinePatchAxisMode` | 3 | 2 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ParticlesCollisionHeightfieldResolution` | 7 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ParticlesCollisionType` | 7 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ParticlesDrawOrder` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ParticlesMode` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ParticlesTransformAlign` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.PrimitiveType` | 6 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ReflectionProbeAmbientMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ReflectionProbeUpdateMode` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.RenderingInfo` | 11 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ShadowCastingSetting` | 4 | 2 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.ShadowQuality` | 7 | 2 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.SkyMode` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.SubSurfaceScatteringQuality` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.TextureLayeredType` | 3 | 4 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.TextureType` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportAnisotropicFiltering` | 6 | 2 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportClearMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportDebugDraw` | 27 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportEnvironmentMode` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportMSAA` | 5 | 3 | 2 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportOcclusionCullingBuildQuality` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportRenderInfo` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportRenderInfoType` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportSDFOversize` | 5 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportSDFScale` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportScaling3DMode` | 6 | 2 | 2 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportScreenSpaceAA` | 4 | 2 | 2 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportUpdateMode` | 5 | 1 | 1 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportVRSMode` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.ViewportVRSUpdateMode` | 4 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.VisibilityRangeFadeMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `RenderingServer.VoxelGIQuality` | 2 | 1 | 0 | 0 | 0 | **到達不可** |
| `Resource.DeepDuplicateMode` | 3 | 1 | 0 | 0 | 0 | — |
| `ResourceLoader.CacheMode` | 5 | 2 | 0 | 0 | 0 | **到達不可** |
| `ResourceLoader.ThreadLoadStatus` | 4 | 0 | 1 | 0 | 0 | **到達不可** |
| `ResourceSaver.SaverFlags` | 8 | 1 | 0 | 0 | 0 | bitfield **到達不可** |
| `RetargetModifier3D.TransformFlag` | 4 | 1 | 1 | 0 | 0 | bitfield |
| `RibbonTrailMesh.Shape` | 2 | 1 | 1 | 0 | 0 | — |
| `RichTextLabel.ImageUpdateMask` | 8 | 1 | 0 | 0 | 0 | bitfield |
| `RichTextLabel.ListType` | 4 | 1 | 0 | 0 | 0 | — |
| `RichTextLabel.MetaUnderline` | 3 | 1 | 0 | 0 | 0 | — |
| `RigidBody2D.CCDMode` | 3 | 1 | 1 | 0 | 0 | — |
| `RigidBody2D.CenterOfMassMode` | 2 | 1 | 1 | 0 | 0 | — |
| `RigidBody2D.DampMode` | 2 | 2 | 2 | 0 | 0 | — |
| `RigidBody2D.FreezeMode` | 2 | 1 | 1 | 0 | 0 | — |
| `RigidBody3D.CenterOfMassMode` | 2 | 1 | 1 | 0 | 0 | — |
| `RigidBody3D.DampMode` | 2 | 2 | 2 | 0 | 0 | — |
| `RigidBody3D.FreezeMode` | 2 | 1 | 1 | 0 | 0 | — |
| `SceneReplicationConfig.ReplicationMode` | 3 | 1 | 1 | 0 | 0 | — |
| `ScriptLanguage.ScriptNameCasing` | 5 | 0 | 0 (1) | 0 | 0 | — |
| `ScrollContainer.ScrollMode` | 5 | 2 | 2 | 0 | 0 | — |
| `Shader.Mode` | 5 | 1 (5) | 1 (2) | 0 | 0 | — |
| `Skeleton3D.ModifierCallbackModeProcess` | 3 | 1 | 1 | 0 | 0 | — |
| `SkeletonModifier3D.BoneAxis` | 6 | 2 | 2 | 0 | 0 | — |
| `SkeletonProfile.TailDirection` | 3 | 1 | 1 | 0 | 0 | — |
| `Sky.ProcessMode` | 4 | 1 | 1 | 0 | 0 | — |
| `Sky.RadianceSize` | 8 | 1 | 1 | 0 | 0 | — |
| `Slider.TickPosition` | 4 | 1 | 1 | 0 | 0 | — |
| `SliderJoint3D.Param` | 23 | 2 | 0 | 0 | 0 | — |
| `SoftBody3D.DisableMode` | 2 | 1 | 1 | 0 | 0 | — |
| `SplitContainer.DraggerVisibility` | 3 | 1 | 1 | 0 | 0 | — |
| `SpringBoneSimulator3D.BoneDirection` | 7 | 1 | 1 | 0 | 0 | — |
| `SpringBoneSimulator3D.CenterFrom` | 3 | 1 | 1 | 0 | 0 | — |
| `SpringBoneSimulator3D.RotationAxis` | 5 | 2 | 2 | 0 | 0 | — |
| `SpriteBase3D.AlphaCutMode` | 4 | 1 | 1 | 0 | 0 | — |
| `SpriteBase3D.DrawFlags` | 6 | 2 | 0 | 0 | 0 | — |
| `StreamPeerTCP.Status` | 4 | 0 | 1 | 0 | 0 | — |
| `StreamPeerTLS.Status` | 5 | 0 | 1 | 0 | 0 | — |
| `StyleBoxTexture.AxisStretchMode` | 3 | 2 | 2 | 0 | 0 | — |
| `SubViewport.ClearMode` | 3 | 1 | 1 | 0 | 0 | — |
| `SubViewport.UpdateMode` | 5 | 1 | 1 | 0 | 0 | — |
| `SurfaceTool.CustomFormat` | 9 | 1 | 1 | 0 | 0 | — |
| `SurfaceTool.SkinWeightCount` | 2 | 1 | 1 | 0 | 0 | — |
| `TabBar.AlignmentMode` | 4 | 2 | 2 | 0 | 0 | — |
| `TabBar.CloseButtonDisplayPolicy` | 4 | 1 | 1 | 0 | 0 | — |
| `TabContainer.TabPosition` | 3 | 1 | 1 | 0 | 0 | — |
| `TextEdit.CaretType` | 2 | 1 | 1 | 0 | 0 | — |
| `TextEdit.EditAction` | 4 | 1 | 0 | 0 | 0 | — |
| `TextEdit.GutterType` | 3 | 1 | 1 | 0 | 0 | — |
| `TextEdit.LineWrappingMode` | 2 | 1 | 1 | 0 | 0 | — |
| `TextEdit.SelectionMode` | 5 | 1 | 1 | 0 | 0 | — |
| `TextServer.AutowrapMode` | 4 | 7 | 7 | 0 | 0 | — |
| `TextServer.Direction` | 4 | 16 (18) | 10 (13) | 0 | 0 | — |
| `TextServer.Feature` | 15 | 1 (2) | 0 | 0 | 0 | — |
| `TextServer.FixedSizeScaleMode` | 3 | 2 (3) | 2 (3) | 0 | 0 | — |
| `TextServer.FontAntialiasing` | 3 | 3 (4) | 3 (4) | 0 | 0 | — |
| `TextServer.FontStyle` | 3 | 2 (3) | 2 (3) | 0 | 0 | bitfield |
| `TextServer.GraphemeFlag` | 14 | 2 (4) | 0 | 0 | 0 | bitfield |
| `TextServer.Hinting` | 3 | 3 (4) | 3 (4) | 0 | 0 | — |
| `TextServer.JustificationFlag` | 9 | 18 (19) | 6 | 0 | 0 | bitfield |
| `TextServer.LineBreakFlag` | 9 | 12 (14) | 5 | 0 | 0 | bitfield |
| `TextServer.Orientation` | 2 | 14 (16) | 3 (4) | 0 | 0 | — |
| `TextServer.OverrunBehavior` | 7 | 7 | 7 | 0 | 0 | — |
| `TextServer.SpacingType` | 5 | 8 (12) | 0 | 0 | 0 | — |
| `TextServer.StructuredTextParser` | 7 | 10 (11) | 8 | 0 | 0 | — |
| `TextServer.SubpixelPositioning` | 6 | 3 (4) | 3 (4) | 0 | 0 | — |
| `TextServer.TextOverrunFlag` | 6 | 1 (2) | 0 | 0 | 0 | bitfield |
| `TextServer.VisibleCharactersBehavior` | 5 | 2 | 2 | 0 | 0 | — |
| `TextureButton.StretchMode` | 7 | 1 | 1 | 0 | 0 | — |
| `TextureLayered.LayeredType` | 3 | 0 | 1 | 0 | 0 | — |
| `TextureRect.ExpandMode` | 6 | 1 | 1 | 0 | 0 | — |
| `TextureRect.StretchMode` | 7 | 1 | 1 | 0 | 0 | — |
| `Theme.DataType` | 7 | 7 | 0 | 0 | 0 | — |
| `Thread.Priority` | 3 | 1 | 0 | 0 | 0 | — |
| `TileMap.VisibilityMode` | 3 | 2 | 2 | 0 | 0 | — |
| `TileMapLayer.DebugVisibilityMode` | 3 | 2 | 2 | 0 | 0 | — |
| `TileSet.CellNeighbor` | 16 | 5 | 0 | 0 | 0 | — |
| `TileSet.TerrainMode` | 3 | 1 | 1 | 0 | 0 | — |
| `TileSet.TileLayout` | 6 | 1 | 1 | 0 | 0 | — |
| `TileSet.TileOffsetAxis` | 2 | 1 | 1 | 0 | 0 | — |
| `TileSet.TileShape` | 4 | 1 | 1 | 0 | 0 | — |
| `TileSetAtlasSource.TileAnimationMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Timer.TimerProcessCallback` | 2 | 1 | 1 | 0 | 0 | — |
| `TouchScreenButton.VisibilityMode` | 2 | 1 | 1 | 0 | 0 | — |
| `Tree.SelectMode` | 3 | 1 | 1 | 0 | 0 | — |
| `TreeItem.TreeCellMode` | 5 | 1 | 1 | 0 | 0 | — |
| `Tween.EaseType` | 4 | 7 (8) | 2 | 0 | 0 | — |
| `Tween.TransitionType` | 12 | 7 (8) | 2 | 0 | 0 | — |
| `Tween.TweenPauseMode` | 3 | 1 | 0 | 0 | 0 | — |
| `Tween.TweenProcessMode` | 2 | 1 | 0 | 0 | 0 | — |
| `UPNPDevice.IGDStatus` | 10 | 1 | 1 | 0 | 0 | — |
| `UndoRedo.MergeMode` | 3 | 2 | 0 | 0 | 0 | — |
| `Viewport.AnisotropicFiltering` | 6 | 1 | 1 | 0 | 0 | — |
| `Viewport.DebugDraw` | 27 | 1 | 1 | 0 | 0 | — |
| `Viewport.DefaultCanvasItemTextureFilter` | 5 | 1 | 1 | 0 | 0 | — |
| `Viewport.DefaultCanvasItemTextureRepeat` | 4 | 1 | 1 | 0 | 0 | — |
| `Viewport.MSAA` | 5 | 2 | 2 | 0 | 0 | — |
| `Viewport.PositionalShadowAtlasQuadrantSubdiv` | 8 | 1 | 1 | 0 | 0 | — |
| `Viewport.RenderInfo` | 4 | 1 | 0 | 0 | 0 | — |
| `Viewport.RenderInfoType` | 4 | 1 | 0 | 0 | 0 | — |
| `Viewport.SDFOversize` | 5 | 1 | 1 | 0 | 0 | — |
| `Viewport.SDFScale` | 4 | 1 | 1 | 0 | 0 | — |
| `Viewport.Scaling3DMode` | 6 | 1 | 1 | 0 | 0 | — |
| `Viewport.ScreenSpaceAA` | 4 | 1 | 1 | 0 | 0 | — |
| `Viewport.VRSMode` | 4 | 1 | 1 | 0 | 0 | — |
| `Viewport.VRSUpdateMode` | 4 | 1 | 1 | 0 | 0 | — |
| `VisibleOnScreenEnabler2D.EnableMode` | 3 | 1 | 1 | 0 | 0 | — |
| `VisibleOnScreenEnabler3D.EnableMode` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShader.Type` | 11 | 16 (19) | 0 | 0 | 0 | — |
| `VisualShader.VaryingMode` | 3 | 1 | 0 | 0 | 0 | — |
| `VisualShader.VaryingType` | 9 | 2 | 1 | 0 | 0 | — |
| `VisualShaderNode.PortType` | 10 | 1 (2) | 1 (4) | 0 | 0 | — |
| `VisualShaderNodeBillboard.BillboardType` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeClamp.OpType` | 7 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeColorFunc.Function` | 7 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeColorOp.Operator` | 10 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeCompare.ComparisonType` | 9 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeCompare.Condition` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeCompare.Function` | 7 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeCubemap.Source` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeCubemap.TextureType` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeDerivativeFunc.Function` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeDerivativeFunc.OpType` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeDerivativeFunc.Precision` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeFloatFunc.Function` | 33 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeFloatOp.Operator` | 11 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeFloatParameter.Hint` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeIntFunc.Function` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeIntOp.Operator` | 13 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeIntParameter.Hint` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeIs.Function` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeMix.OpType` | 8 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeMultiplyAdd.OpType` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeParameter.Qualifier` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeParticleAccelerator.Mode` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeParticleEmit.EmitFlags` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeParticleRandomness.OpType` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeRemap.OpType` | 8 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeSample3D.Source` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeSmoothStep.OpType` | 8 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeStep.OpType` | 8 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeSwitch.OpType` | 9 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTexture.Source` | 9 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTexture.TextureType` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTextureParameter.ColorDefault` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTextureParameter.TextureFilter` | 8 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTextureParameter.TextureRepeat` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTextureParameter.TextureSource` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTextureParameter.TextureType` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTransformFunc.Function` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTransformOp.Operator` | 10 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeTransformVecMult.Operator` | 5 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeUIntFunc.Function` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeUIntOp.Operator` | 13 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeUVFunc.Function` | 3 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeVectorBase.OpType` | 4 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeVectorFunc.Function` | 34 | 1 | 1 | 0 | 0 | — |
| `VisualShaderNodeVectorOp.Operator` | 13 | 1 | 1 | 0 | 0 | — |
| `VoxelGI.Subdiv` | 5 | 1 | 1 | 0 | 0 | — |
| `WebRTCDataChannel.ChannelState` | 4 | 0 | 1 (2) | 0 | 0 | — |
| `WebRTCDataChannel.WriteMode` | 2 | 1 (2) | 1 (2) | 0 | 0 | — |
| `WebRTCPeerConnection.ConnectionState` | 6 | 0 | 1 (2) | 0 | 0 | — |
| `WebRTCPeerConnection.GatheringState` | 3 | 0 | 1 (2) | 0 | 0 | — |
| `WebRTCPeerConnection.SignalingState` | 6 | 0 | 1 (2) | 0 | 0 | — |
| `WebSocketPeer.State` | 4 | 0 | 1 | 0 | 0 | — |
| `WebSocketPeer.WriteMode` | 2 | 1 | 0 | 0 | 0 | — |
| `WebXRInterface.TargetRayMode` | 4 | 0 | 1 | 0 | 0 | — |
| `Window.ContentScaleAspect` | 5 | 1 | 1 | 0 | 0 | — |
| `Window.ContentScaleMode` | 3 | 1 | 1 | 0 | 0 | — |
| `Window.ContentScaleStretch` | 2 | 1 | 1 | 0 | 0 | — |
| `Window.Flags` | 14 | 2 | 0 | 0 | 0 | — |
| `Window.LayoutDirection` | 7 | 1 | 1 | 0 | 0 | — |
| `Window.Mode` | 5 | 1 | 1 | 0 | 0 | — |
| `Window.WindowInitialPosition` | 6 | 1 | 1 | 0 | 0 | — |
| `XMLParser.NodeType` | 7 | 0 | 1 | 0 | 0 | — |
| `XRBodyModifier3D.BodyUpdate` | 3 | 1 | 1 | 0 | 0 | bitfield |
| `XRBodyModifier3D.BoneUpdate` | 3 | 1 | 1 | 0 | 0 | — |
| `XRBodyTracker.BodyFlags` | 3 | 1 | 1 | 0 | 0 | bitfield |
| `XRBodyTracker.Joint` | 88 | 4 | 0 | 0 | 0 | — |
| `XRBodyTracker.JointFlags` | 4 | 1 | 1 | 0 | 0 | bitfield |
| `XRFaceTracker.BlendShapeEntry` | 144 | 2 | 0 | 0 | 0 | — |
| `XRHandModifier3D.BoneUpdate` | 3 | 1 | 1 | 0 | 0 | — |
| `XRHandTracker.HandJoint` | 27 | 10 | 0 | 0 | 0 | — |
| `XRHandTracker.HandJointFlags` | 6 | 1 | 1 | 0 | 0 | bitfield |
| `XRHandTracker.HandTrackingSource` | 5 | 1 | 1 | 0 | 0 | — |
| `XRInterface.EnvironmentBlendMode` | 3 | 1 | 1 | 0 | 0 | — |
| `XRInterface.PlayAreaMode` | 6 | 2 (4) | 1 (2) | 0 | 0 | — |
| `XRInterface.TrackingStatus` | 5 | 0 | 1 (2) | 0 | 0 | — |
| `XRInterface.VRSTextureFormat` | 3 | 0 | 0 (1) | 0 | 0 | — |
| `XRPose.TrackingConfidence` | 3 | 2 | 2 | 0 | 0 | — |
| `XRPositionalTracker.TrackerHand` | 4 | 1 | 2 | 0 | 0 | — |
| `XRServer.RotationMode` | 3 | 1 | 0 | 0 | 0 | **到達不可** |
| `XRServer.TrackerType` | 10 | 1 | 1 | 0 | 0 | **到達不可** |
| `ZIPPacker.ZipAppend` | 3 | 1 | 0 | 0 | 0 | — |

## どこからも名指されないもの

エンジン API のどこにも現れない。GDScript から見ても値として受け取ることも渡すこともできない、C++ の内部のためだけの定数である。

| 列挙体 | 値 | 引数 | 返り | プロパティ | シグナル | 印 |
| --- | --- | --- | --- | --- | --- | --- |
| `MethodFlags` | 9 | 0 | 0 | 0 | 0 | bitfield **未使用** |
| `Orientation` | 2 | 0 | 0 | 0 | 0 | **未使用** |
| `Variant.Operator` | 26 | 0 | 0 | 0 | 0 | **未使用** |
| `AnimationNodeOneShot.OneShotRequest` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `BaseMaterial3D.StencilFlags` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `CodeEdit.CodeCompletionLocation` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `Control.Anchor` | 2 | 0 | 0 | 0 | 0 | **未使用** |
| `DisplayServer.AccessibilityScrollHint` | 6 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `DisplayServer.AccessibilityScrollUnit` | 2 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `DisplayServer.WindowEvent` | 9 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `ENetConnection.EventType` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `EditorExportPreset.ScriptExportMode` | 3 | 0 | 0 | 0 | 0 | **未使用** editor |
| `EditorPlugin.AfterGUIInput` | 3 | 0 | 0 | 0 | 0 | **未使用** editor |
| `EditorScenePostImportPlugin.InternalImportCategory` | 8 | 0 | 0 | 0 | 0 | **未使用** editor |
| `EditorUndoRedoManager.SpecialHistory` | 3 | 0 | 0 | 0 | 0 | **未使用** editor |
| `GPUParticles2D.EmitFlags` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `GPUParticles3D.EmitFlags` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `Geometry2D.PolyBooleanOperation` | 4 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `HTTPClient.ResponseCode` | 61 | 0 | 0 | 0 | 0 | **未使用** |
| `HTTPRequest.Result` | 14 | 0 | 0 | 0 | 0 | **未使用** |
| `JSONRPC.ErrorCode` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `LightmapGI.BakeError` | 12 | 0 | 0 | 0 | 0 | **未使用** |
| `LineEdit.MenuItems` | 32 | 0 | 0 | 0 | 0 | **未使用** |
| `Logger.ErrorType` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `Mesh.ArrayCustomFormat` | 9 | 0 | 0 | 0 | 0 | **未使用** |
| `Mesh.ArrayType` | 14 | 0 | 0 | 0 | 0 | **未使用** |
| `MultiplayerAPI.RPCMode` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `NavigationPathQueryResult2D.PathSegmentType` | 2 | 0 | 0 | 0 | 0 | **未使用** |
| `NavigationPathQueryResult3D.PathSegmentType` | 2 | 0 | 0 | 0 | 0 | **未使用** |
| `Node.DuplicateFlags` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `OS.RenderingDriver` | 4 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `Object.ConnectFlags` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `OpenXRInterface.PerfSettingsNotificationLevel` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `OpenXRInterface.PerfSettingsSubDomain` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `PhysicsServer2D.AreaBodyStatus` | 2 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `PhysicsServer2D.AreaSpaceOverrideMode` | 5 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `PhysicsServer2D.BodyDampMode` | 2 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `PhysicsServer3D.AreaBodyStatus` | 2 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `PhysicsServer3D.AreaSpaceOverrideMode` | 5 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `PhysicsServer3D.BodyDampMode` | 2 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `ProgressBar.FillMode` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `RenderingDevice.BreadcrumbMarker` | 13 | 0 | 0 | 0 | 0 | **未使用** |
| `RenderingDevice.PipelineSpecializationConstantType` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `RenderingServer.ArrayCustomFormat` | 9 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.ArrayType` | 14 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.BakeChannels` | 4 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.CubeMapLayer` | 6 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.InstanceType` | 15 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.LightType` | 3 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.PipelineSource` | 6 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `RenderingServer.ShaderMode` | 6 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `ResourceFormatLoader.CacheMode` | 5 | 0 | 0 | 0 | 0 | **未使用** |
| `ResourceImporter.ImportOrder` | 2 | 0 | 0 | 0 | 0 | **未使用** |
| `RichTextLabel.MenuItems` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `SceneState.GenEditState` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `SceneTree.GroupCallFlags` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `ScriptLanguageExtension.CodeCompletionKind` | 11 | 0 | 0 | 0 | 0 | **未使用** |
| `ScriptLanguageExtension.CodeCompletionLocation` | 4 | 0 | 0 | 0 | 0 | **未使用** |
| `ScriptLanguageExtension.LookupResultType` | 12 | 0 | 0 | 0 | 0 | **未使用** |
| `TextEdit.MenuItems` | 32 | 0 | 0 | 0 | 0 | **未使用** |
| `TextEdit.SearchFlags` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `TextServer.ContourPointTag` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `TextServer.FontLCDSubpixelLayout` | 6 | 0 | 0 | 0 | 0 | **未使用** |
| `TextureProgressBar.FillMode` | 9 | 0 | 0 | 0 | 0 | **未使用** |
| `Time.Month` | 12 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `Time.Weekday` | 7 | 0 | 0 | 0 | 0 | **未使用** **到達不可** |
| `Tree.DropModeFlags` | 3 | 0 | 0 | 0 | 0 | **未使用** |
| `UPNP.UPNPResult` | 29 | 0 | 0 | 0 | 0 | **未使用** |
| `XRInterface.Capabilities` | 7 | 0 | 0 | 0 | 0 | **未使用** |
| `ZIPPacker.CompressionLevel` | 4 | 0 | 0 | 0 | 0 | **未使用** |
