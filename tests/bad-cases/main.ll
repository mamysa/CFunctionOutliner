; ModuleID = 'main.c'
source_filename = "main.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.external = type { i32, i32 }
%struct.anon = type { i32, float }
%struct.internal = type { i32, float }

@main.mystruct1 = private unnamed_addr constant %struct.external { i32 1, i32 5 }, align 4
@main.mystruct2 = private unnamed_addr constant %struct.anon { i32 1, float 0x4016666660000000 }, align 4
@main.mystruct3 = private unnamed_addr constant %struct.internal { i32 4, float 0x4016666660000000 }, align 4

; Function Attrs: nounwind uwtable
define i32 @main() #0 !dbg !6 {
entry:
  %retval = alloca i32, align 4
  %mystruct1 = alloca %struct.external, align 4
  %mystruct2 = alloca %struct.anon, align 4
  %mystruct3 = alloca %struct.internal, align 4
  store i32 0, i32* %retval, align 4
  call void @llvm.dbg.declare(metadata %struct.external* %mystruct1, metadata !10, metadata !15), !dbg !16
  %0 = bitcast %struct.external* %mystruct1 to i8*, !dbg !16
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* bitcast (%struct.external* @main.mystruct1 to i8*), i64 8, i32 4, i1 false), !dbg !16
  call void @llvm.dbg.declare(metadata %struct.anon* %mystruct2, metadata !17, metadata !15), !dbg !23
  %1 = bitcast %struct.anon* %mystruct2 to i8*, !dbg !23
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %1, i8* bitcast (%struct.anon* @main.mystruct2 to i8*), i64 8, i32 4, i1 false), !dbg !23
  call void @llvm.dbg.declare(metadata %struct.internal* %mystruct3, metadata !24, metadata !15), !dbg !29
  %2 = bitcast %struct.internal* %mystruct3 to i8*, !dbg !29
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %2, i8* bitcast (%struct.internal* @main.mystruct3 to i8*), i64 8, i32 4, i1 false), !dbg !29
  ret i32 0, !dbg !30
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #2

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { argmemonly nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}
!llvm.ident = !{!5}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 4.0.0 (trunk 284302)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
!1 = !DIFile(filename: "main.c", directory: "/home/alex/UNIPROJECT2/llvm/lib/Transforms/FuncExtract/tests/bad-cases")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{!"clang version 4.0.0 (trunk 284302)"}
!6 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 2, type: !7, isLocal: false, isDefinition: true, scopeLine: 2, isOptimized: false, unit: !0, variables: !2)
!7 = !DISubroutineType(types: !8)
!8 = !{!9}
!9 = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!10 = !DILocalVariable(name: "mystruct1", scope: !6, file: !1, line: 3, type: !11)
!11 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "external", file: !1, line: 1, size: 64, align: 32, elements: !12)
!12 = !{!13, !14}
!13 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !11, file: !1, line: 1, baseType: !9, size: 32, align: 32)
!14 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !11, file: !1, line: 1, baseType: !9, size: 32, align: 32, offset: 32)
!15 = !DIExpression()
!16 = !DILocation(line: 3, column: 18, scope: !6)
!17 = !DILocalVariable(name: "mystruct2", scope: !6, file: !1, line: 4, type: !18)
!18 = distinct !DICompositeType(tag: DW_TAG_structure_type, scope: !6, file: !1, line: 4, size: 64, align: 32, elements: !19)
!19 = !{!20, !21}
!20 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !18, file: !1, line: 4, baseType: !9, size: 32, align: 32)
!21 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !18, file: !1, line: 4, baseType: !22, size: 32, align: 32, offset: 32)
!22 = !DIBasicType(name: "float", size: 32, align: 32, encoding: DW_ATE_float)
!23 = !DILocation(line: 4, column: 28, scope: !6)
!24 = !DILocalVariable(name: "mystruct3", scope: !6, file: !1, line: 5, type: !25)
!25 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "internal", scope: !6, file: !1, line: 5, size: 64, align: 32, elements: !26)
!26 = !{!27, !28}
!27 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !25, file: !1, line: 5, baseType: !9, size: 32, align: 32)
!28 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !25, file: !1, line: 5, baseType: !22, size: 32, align: 32, offset: 32)
!29 = !DILocation(line: 5, column: 37, scope: !6)
!30 = !DILocation(line: 6, column: 2, scope: !6)
