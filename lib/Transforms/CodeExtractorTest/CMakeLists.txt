add_llvm_loadable_module(CodeExtractorTest
	CodeExtractorTest.cpp

	ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
)
