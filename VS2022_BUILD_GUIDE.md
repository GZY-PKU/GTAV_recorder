# Visual Studio 2022 编译 GTA5 Script Hook V 插件指南

## 1. 创建 VS 项目

### 步骤 1: 创建空项目
1. 打开 Visual Studio 2022
2. 选择 "创建新项目"
3. 选择 "空项目" (Empty Project)
4. 项目名称: `GTAV_Recorder`
5. 位置: `E:\isggg\cwz\GTAV_Recorder\build\` (或任意位置)
6. 勾选 "将解决方案和项目放在同一目录中"

### 步骤 2: 添加源文件
1. 在解决方案资源管理器中，右键 "源文件" → "添加" → "现有项"
2. 选择 `E:\isggg\cwz\GTAV_Recorder\source\main.cpp`

### 步骤 3: 添加头文件路径
1. 右键项目 → "属性" (Properties)
2. 配置: `所有配置` (All Configurations)
3. 平台: `所有平台` (All Platforms)
4. 导航到: `C/C++` → `所有选项` → `附加包含目录`
5. 添加: `E:\isggg\cwz\GTAV_Recorder\SDK\inc`

## 2. 配置项目属性 

### 步骤 1: 配置类型改为 DLL
1. 项目属性 → `常规` (General)
2. `配置类型` → `动态库(.dll)`

### 步骤 2: 设置输出文件名
1. 项目属性 → `常规`
2. `目标文件名` → `GTAV_Recorder` (不要 .dll 后缀，VS 会自动添加)
3. `目标文件扩展名` → `.asi` //2026版本在高级里

或者更简单的方法：
1. 项目属性 → `链接器` → `常规`
2. `输出文件` → `$(OutDir)GTAV_Recorder.asi`

### 步骤 3: 平台配置为 x64
1. 顶部工具栏: `x86` 改为 `x64`
2. 确保所有配置都是 x64 (GTA5 是 64 位游戏)

### 步骤 4: C++ 标准设置
1. 项目属性 → `C/C++` → `语言`
2. `C++ 语言标准` → `ISO C++17 标准 (/std:c++17)` 或 `ISO C++14`

### 步骤 5: 关闭预编译头
1. 项目属性 → `C/C++` → `预编译头`
2. `预编译头` → `不使用预编译头`

//补充：

1. 右键项目 → `属性`
2. `链接器` → `常规` → `附加库目录`
3. 添加：`E:\isggg\cwz\GTAV_Recorder\SDK\lib`

4. `链接器` → `输入` → `附加依赖项`

5. 在原有内容后面添加（注意分号分隔）：

```
ScriptHookV.lib;User32.lib
```

## 3. 编译配置     //还是刚才那块

### Debug 配置
1. 切换到 `Debug` 配置
2. 项目属性 → `C/C++` → `优化` → `优化` → `已禁用 (/Od)`
3. 项目属性 → `C/C++` → `代码生成` → `运行库` → `多线程调试 DLL (/MDd)`

### Release 配置
1. 切换到 `Release` 配置
2. 项目属性 → `C/C++` → `优化` → `优化` → `使大小最小化或使速度最大化 (/O2)`
3. 项目属性 → `C/C++` → `代码生成` → `运行库` → `多线程 DLL (/MD)`

## 4. 编译

1. 选择 `Release` + `x64`
2. 菜单栏 → `生成` → `生成解决方案` (或按 F7)
3. 输出文件位置: `E:\isggg\cwz\GTAV_Recorder\build\x64\Release\GTAV_Recorder.asi`

## 5. 安装到 GTA5

1. 复制 `GTAV_Recorder.asi` 到 GTA5 游戏根目录
2. 确保已安装 Script Hook V:
   - `dinput8.dll` 在游戏根目录
   - `ScriptHookV.dll` 在游戏根目录

## 6. 调试 (可选)

### 附加到进程调试
1. 启动 GTA5
2. VS 菜单: `调试` → `附加到进程`
3. 找到 `GTA5.exe` → `附加`
4. 在代码中设置断点
5. 在游戏中触发对应功能

### 日志查看
- 日志文件: `GTAV_Recorder_Debug.log` (在 GTA5 根目录生成)

## 快速检查清单

- [ ] 项目是 x64 平台
- [ ] 配置类型是 DLL
- [ ] 输出扩展名是 .asi
- [ ] 包含目录添加了 SDK/inc
- [ ] C++ 标准是 C++14 或更高
- [ ] 关闭预编译头
- [ ] Release 模式使用 /MD
- [ ] Debug 模式使用 /MDd

## 常见问题

### Q: 提示找不到 natives.h
A: 检查 "附加包含目录" 是否正确设置为 `E:\isggg\cwz\GTAV_Recorder\SDK\inc`

### Q: 编译成功但游戏不加载
A: 1. 确认是 x64 Release 编译
   2. 确认输出文件是 .asi 扩展名
   3. 确认 Script Hook V 已正确安装

### Q: 运行时崩溃
A: 1. 使用 Debug 模式编译，附加调试器查看错误
   2. 检查日志文件 GTAV_Recorder_Debug.log
