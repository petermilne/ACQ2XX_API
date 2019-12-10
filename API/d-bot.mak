# d-tacq projects bottom level makefile include
# $file:$


# Rules for compiling source files to object files
$(OBJDIR)/%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

$(OBJDIR)/%.o : %.c
	${CC} -c ${CFLAGS} ${INCS} $< -o $@


