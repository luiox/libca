# Task Prompt Log

## 2026-04-01 任务输入 001
```
# 开发约束
【任务开始】创建临时文件 `task_plan.md`，通过跟用户交互式提问，明确任务需求，并且记录需求和开发计划。
# 强制原则
- 代码即事实：文档必须追随代码的变化而更新。
- 闭环原则：禁止在文档更新完成前删除临时文件或结束任务。
- prompt记录：将用户每次的prompt输入进临时文件`task_prompt.md`文件内。
---
请你使用gh cli 拉取 https://github.com/luiox/libca/issues/81 这个issue的内容，分析需求和目标功能，有问题交互式跟我确定。
```

## 2026-04-01 交互确认 002
```
兼容默认行为: 是，保持兼容并默认 std
非法值策略: 回退到 std 并给 warning
测试范围: 是，补齐 4 组合
文档范围: 更新em_base的设计文档，仅仅添加两个宏的说明，具体怎么使用放在使用文档内说明
```

## 2026-04-01 任务输入 003
```
开一个对应的issue的分支，把当前的修改带过去，然后使用 commit skill进行撰写提交信息，并且使用gh cli推送此commit。我注意到你这个当前em_base的单元测试加入em_base的时候，没有走源码包管理模式，没有把自己作为一个user，而是直接操作加红了，不过我的想法是，再加一组用户版的测试，比如两个都用custom，两个都用std模式的，这样子可以测试出用户使用的问题。完成这个任务以后再提交一次。结束的时候，交互式跟我确定，必须我验收以后才能认为任务完成。
```

## 2026-04-01 任务输入 004
```
临时文件`task_plan.md`和`task_prompt.md`应该被加入git管理。
当年把这两个文件加入git并且commit的时候，我确定你已经完成任务，你应该将当前的`task_plan.md`和`task_prompt.md`文件移动进仓库的`archieve/issue-xxx`这个目录下，其中xxx为issue号，最终为`archieve/issue-xxx/task_plan.md`这样子的路径。
然后请你使用gh cli工具开一个对应的pr，然后使用 personification skill进行撰写内容。
```
