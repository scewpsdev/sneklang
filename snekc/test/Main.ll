; ModuleID = 'Main.c'
source_filename = "Main.c"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.29.30133"

%struct.Test = type { i32, float }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @abc() #0 !dbg !8 {
  ret void, !dbg !11
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 !dbg !12 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Test, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  call void @llvm.dbg.declare(metadata %struct.Test* %2, metadata !16, metadata !DIExpression()), !dbg !22
  call void @llvm.dbg.declare(metadata i32* %3, metadata !23, metadata !DIExpression()), !dbg !24
  store i32 5, i32* %3, align 4, !dbg !24
  ret i32 0, !dbg !25
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 11.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None, sysroot: "/")
!1 = !DIFile(filename: "Main.c", directory: "D:\\Projects\\sneklang6\\snekc\\test", checksumkind: CSK_MD5, checksum: "351c06a4ed1b44fe58c785721f0d7fba")
!2 = !{}
!3 = !{i32 2, !"CodeView", i32 1}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 2}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{!"clang version 11.0.0"}
!8 = distinct !DISubprogram(name: "abc", scope: !1, file: !1, line: 7, type: !9, scopeLine: 8, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!9 = !DISubroutineType(types: !10)
!10 = !{null}
!11 = !DILocation(line: 10, scope: !8)
!12 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 12, type: !13, scopeLine: 13, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!13 = !DISubroutineType(types: !14)
!14 = !{!15}
!15 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!16 = !DILocalVariable(name: "test", scope: !12, file: !1, line: 14, type: !17)
!17 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Test", file: !1, line: 1, size: 64, elements: !18)
!18 = !{!19, !20}
!19 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !17, file: !1, line: 3, baseType: !15, size: 32)
!20 = !DIDerivedType(tag: DW_TAG_member, name: "f", scope: !17, file: !1, line: 4, baseType: !21, size: 32, offset: 32)
!21 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!22 = !DILocation(line: 14, scope: !12)
!23 = !DILocalVariable(name: "i", scope: !12, file: !1, line: 15, type: !15)
!24 = !DILocation(line: 15, scope: !12)
!25 = !DILocation(line: 16, scope: !12)
