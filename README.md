# MIT 6.S081

## 前言

​	10天，小学期啥也没干，就上了这门课。直观感受是这门课很难啊（~~不过我也确实菜~~🥲），课程视频其实边听边走神，关于xv6的原理还是得靠 xv6 book；lab做的有点稀碎哈，一开始对这种系统编程没有经验，无从下手，后来能跟着hint写点自己的想法，但是遇到需要自己决策的lab还是参考大佬的博客，不过倒是敢写代码了，不至于上来蒙的不敢写。

## 课程细节

​	这门课没有设计太多关于OS的**详细理论**，侧重点在于**分析xv6**，讲述xv6的设计原理，lab也是在xv6的基础上进行修改，只能说很**注重实践**了，个人感觉最有用的还是 [xv6 book](https://pdos.csail.mit.edu/6.1810/2023/xv6/book-riscv-rev3.pdf),读不下去英文的可以看[中文文档](https://th0ar.gitbooks.io/xv6-chinese/content/index.html)。

​	这门课的后半部分都在分析一些**OS论文**，涉及到的概念有：**虚拟机，微内核，meltdown计算机攻击，RCU**等，涉猎的还是很广泛的，相比第一部分，其实我更喜欢第二部分

## 修读建议

​	如果你还没有了解过OS一些机制，我建议你在上这门课之前先去读 **OSTEP** 操作系统导论，否则的话vm，lock，fs这些概念可能一时间无法接受(其实到现在我觉得自己也没有完全理解这些概念，只能干中学了🥺)，读完OSTEP你能对这些概念的发展有个更清晰的脉络。

​	前面已经说了这门课注重实践多一点，所以要 **注意了解xv6的一些运行机制**，比如说 **trap，driver，fs**这些

## 关于lab

​	恕博主无能，实在是能力有限，很多lab都是靠参考才一点一点敲出来，有的甚至都没有make成功或者通过测试，但是我主要目的在于了解一些code组织，所以没有费心费力去纠正这些bug，大家有能力就不要学我了😭

​	所以**不建议**参考我的lab实现，我上传到github主要目的是记录学习过程，所以有仓库里有一些零散的**note**和 **lab手稿**

​	以下列出了我在做lab的一些参考，感谢这些博主🙏

- [tzyt](https://ttzytt.com/tags/xv6/)
- [KuangjuX](https://github.com/KuangjuX/xv6-riscv-solution)
- [PKUFlyingPig](https://github.com/PKUFlyingPig/MIT6.S081-2020fall)
- [llzdz](https://blog.csdn.net/weixin_42543071/article/details/143351746)
- [Rinai_R](https://blog.csdn.net/qq_60409213/article/details/147146553?ops_request_misc=%257B%2522request%255Fid%2522%253A%25226dd9a9b840f35e33f8cca03825d5b5f6%2522%252C%2522scm%2522%253A%252220140713.130102334..%2522%257D&request_id=6dd9a9b840f35e33f8cca03825d5b5f6&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduend~default-2-147146553-null-null.142^v102^pc_search_result_base2&utm_term=xv6%20lab3&spm=1018.2226.3001.4187)

## 最后

​	这一阶段就到这里啦，马上要放暑假了，我的大一就要结束了。还在考虑要不要听一听南大jyy的OS课，或者开始学计网呢。希望以后要坚持开心学cs😇
