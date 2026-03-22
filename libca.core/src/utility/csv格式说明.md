### CSV的格式规范

下面的格式规范定义来源于RFC 4180，附上原文供参考，一共也就七点。

**1. 每一行记录位于一个单独的行上，用回车换行符CRLF(也就是\r\n)分割。**

> Each record is located on a separate line, delimited by a line break (CRLF). For example:
>
> ```
> aaa,bbb,ccc CRLF
> zzz,yyy,xxx CRLF12
> ```

**2. 文件中的最后一行记录可以有结尾回车换行符，也可以没有。**

> The last record in the file may or may not have an ending line break. For example:
>
> ```
> aaa,bbb,ccc CRLF
> zzz,yyy,xxx12
> ```

**3. 第一行可以存在一个可选的标题头，格式和普通记录行的格式一样。标题头要包含文件记录字段对应的名称，应该有和记录字段一样的数量。（在MIME类型中，标题头行的存在与否可以通过MIME type中的可选”header”参数指明）**

> There maybe an optional header line appearing as the first line of the file with the same format as normal record lines. This header will contain names corresponding to the fields in the file and should contain the same number of fields as the records in the rest of the file (the presence or absence of the header line should be indicated via the optional “header” parameter of this MIME type). For example:
>
> ```
> field_name,field_name,field_name CRLF
> aaa,bbb,ccc CRLF
> zzz,yyy,xxx CRLF123
> ```

**4. 在标题头行和普通行每行记录中，会存在一个或多个由半角逗号(,)分隔的字段。整个文件中每行应包含相同数量的字段，空格也是字段的一部分，不应被忽略。每一行记录最后一个字段后不能跟逗号。（通常用逗号分隔，也有其他字符分隔的CSV，需事先约定）**

> Within the header and each record, there may be one or more fields, separated by commas. Each line should contain the same number of fields throughout the file. Spaces are considered part of a field and should not be ignored. The last field in the record must not be followed by a comma. For example:
>
> ```
> aaa,bbb,ccc1
> ```

**5. 每个字段可用也可不用半角双引号(“)括起来（不过有些程序，如Microsoft的Excel就根本不用双引号）。如果字段没有用引号括起来，那么该字段内部不能出现双引号字符。**

> Each field may or may not be enclosed in double quotes (however some programs, such as Microsoft Excel, do not use double quotes at all). If fields are not enclosed with double quotes, then double quotes may not appear inside the fields. For example:
>
> ```
> "aaa","bbb","ccc" CRLF
> zzz,yyy,xxx12
> ```

**6. 字段中若包含回车换行符、双引号或者逗号，该字段需要用双引号括起来。**

> Fields containing line breaks (CRLF), double quotes, and commas should be enclosed in double-quotes. For example:*（下面原文的例子可能有些问题）*
>
> ```
> "aaa","b CRLF
> bb","ccc" CRLF
> zzz,yyy,xxx123
> ```

**7. 如果用双引号括字段，那么出现在字段内的双引号前必须加一个双引号进行转义。**

> If double-quotes are used to enclose fields, then a double-quote appearing inside a field must be escaped by preceding it with another double quote. For example:
>
> ```
> "aaa","b""bb","ccc"
> ```
