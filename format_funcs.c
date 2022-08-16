#include "format_funcs.h"
#include <string.h>

typedef struct info_format_function_arg* Arg;
#define RET_NOTHING return List_create(sizeof(struct info_internal_drawcall));

char last_prefix_buff[512]={0};
size_t last_prefix_buff_length=0;

static bool util_strcmp(const char *a, const char *b, size_t len)
{
        for(size_t i=0; i<len; i++)
                if(a!=b)
                        return false;
        return true;
}

static List timestamp(List args)
{
        const char *fmt = "%d:%.2d:%.2d";
        // check args
        if(List_size(args)!=0){
                INTERNAL_ERROR("got %d args but take at most 1", List_size(args))
                return NULL;
        }

        if(List_size(args)==1){
                Arg arg = List_get(args,0);
                if(arg->type!=BUFFER){
                        INTERNAL_ERROR("wrong arg type! Need BUFFER arg")
                        return NULL;
                }
                fmt = info_internal_buffer_str(arg->buf);
        }

        List drawcall_list = List_create(sizeof( struct info_internal_drawcall));

        time_t time_rel;
        struct tm *time_struct;
        time(&time_rel);
        time_struct = gmtime(&time_rel);
        if(info_internal_drawcall_printf(drawcall_list, TEXT, ansi_prefix, fmt, (int)time_struct->tm_hour, (int)time_struct->tm_min, (int)time_struct->tm_sec))
                INTERNAL_ERROR("print failed")
        return drawcall_list;
}

List substring_tmp = NULL;
static List substring_start(List args)
{
        if(!substring_tmp)
                substring_tmp = List_create(sizeof(struct info_internal_format_substring));

        struct info_internal_format_substring substring = {0};
        substring.offset_start = info_internal_buffer_tell(formatting_info.buffer);
        if(List_size(args)>1){
                INTERNAL_ERROR("got %d args but take at most 1", List_size(args))
                return NULL;
        }

        if(List_size(args)==1){
                Arg arg = List_get(args,0);
                if(arg->type!=BUFFER){
                        INTERNAL_ERROR("wrong arg type! Need BUFFER arg")
                        return NULL;
                }
                substring.name=info_internal_buffer_str(arg->buf);
        }



        List_append(substring_tmp, &substring);
        RET_NOTHING
}

static List substring_end(List args)
{
        if(List_size(args)){
                INTERNAL_ERROR("got %d args but need 0", List_size(args))
                return NULL;
        }

        if(!List_size(substring_tmp)){
                INTERNAL_ERROR("Expected '(' before ')'")
                return NULL;
        }

        struct info_internal_format_substring *substring = List_pop(substring_tmp);
        substring->offset_end = info_internal_buffer_tell(formatting_info.buffer);

        List_append(formatting_info.substrings, substring);

        if(!List_size(substring_tmp)){
                List_free(substring_tmp);
                substring_tmp=NULL;
        }
        RET_NOTHING
}


static List info_tag(List args)
{
        List drawcall_list = List_create(sizeof(struct info_internal_drawcall));
        info_internal_drawcall_printf(drawcall_list, TEXT, info_ANSI[formatting_info.current->type],  info_internal_type_ids[formatting_info.current->type]);
        return drawcall_list;
}

static List info_content(List args)
{
        size_t argc = List_size(args);
        if(argc>1){
                INTERNAL_ERROR("context takes at most one argument, but got %d", argc)
                return NULL;
        }
        enum info_internal_drawcall_content_stream stream = TEXT;
        if(argc==1)
        {
                Arg arg = List_get(args, 0);
                if(arg->type!=INT)
                {
                        INTERNAL_ERROR("context takes INT Variable only")
                        return NULL;
                }
                stream = arg->num;
        }

        List elements = formatting_info.current->drawcall_list,
             out = List_create(sizeof(struct info_internal_drawcall));

        for(struct info_internal_drawcall *start=List_start(elements),
                        *end = List_end(elements);
                        start!=end; start++)
        {
                if(start->content_stream==stream){
                        struct info_internal_drawcall *d = List_append(out, NULL);
                        d->ansi=start->ansi;
                        d->content_stream=stream;
                        d->content=info_internal_buffer_create(20);
                        info_internal_buffer_append(d->content, info_internal_buffer_str(start->content), info_internal_buffer_tell(start->content));
                }
        }
        return out;

}

List info_funcname(List args)
{
        if(List_size(args)){
                INTERNAL_ERROR("func name doesn't take any arguments")
                return NULL;
        }
        List out = List_create(sizeof(struct info_internal_drawcall));
        info_internal_drawcall_printf(out, TEXT, ansi_prefix, formatting_info.current->origin.func);
        return out;
}

static List info_whitespaces(List args)
{

        if(List_size(args)!=1) {
                INTERNAL_ERROR("takes 1 argument, but got %d", List_size(args))
                return NULL;
        }
        Arg arg = List_get(args, 0);

        if(arg->type!=BUFFER){
                INTERNAL_ERROR("argument has to be string")
                return NULL;
        }

        List out = List_create(sizeof(struct info_internal_drawcall));

        size_t sstrlen =info_internal_buffer_tell(arg->buf);
        struct info_internal_drawcall *d = List_append(out, NULL);
        d->ansi=formatting_info.current->start;
        d->content_stream=TEXT;
        d->content=info_internal_buffer_create(sstrlen);

        char *out_str = info_internal_buffer_str(d->content);
        const char *in_str = info_internal_buffer_str(arg->buf);
        bool skip=false;
        int pos = 0;
        for(int i=0; i<sstrlen; i++)
        {
                if(in_str[i]=='\033') skip=true;

                if(skip || in_str[i]==0){
                        if(in_str[i]=='m') skip=false;
                }else{
                        if(in_str[i]=='\t' || in_str[i]=='\n')
                                out_str[pos]==in_str[i];
                        else
                                out_str[pos]=' ';
                        pos++;
                }
        }
        info_internal_buffer_seek(d->content, pos);
        return out;
}

static List info_indentation(List args)
{
        if(List_size(args)){
                INTERNAL_ERROR("dont take any args")
                return NULL;
        }

        // save prefix
        const char * prefix_str = info_internal_buffer_str(formatting_info.buffer);
        size_t lenght = info_internal_buffer_tell(formatting_info.buffer);
        lenght = lenght<512 ? lenght : 512;
        last_prefix_buff_length=lenght;
        for(int i=0; i<lenght; i++)
                last_prefix_buff[i]=prefix_str[i];

        unsigned short indentation = formatting_info.current->indentation;
        List out = List_create(sizeof(struct info_internal_drawcall));
        struct info_internal_drawcall *d = List_append(out, NULL);
        d->ansi=formatting_info.current->start;
        d->content_stream=TEXT;
        d->content=info_internal_buffer_create(indentation);

        char *str = info_internal_buffer_str(d->content);
        for(unsigned short i=0; i<indentation; i++)
                str[i]='\t';
        info_internal_buffer_seek(d->content, indentation);
        return out;
}

static List info_substring_get(List args)
{
        int index=-1;
        if(!List_size(formatting_info.substrings)){
                INTERNAL_ERROR("No Subtrings where found")
                return NULL;
        }
        // check args
        if(List_size(args)>1){
                INTERNAL_ERROR("got %d args but take at most 1", List_size(args))
                return NULL;
        }
        if(List_size(args)==1){
                Arg arg = List_get(args,0);
                if(arg->type==INT){
                        index=arg->num;
                        if(index>List_size(formatting_info.substrings)){
                                INTERNAL_ERROR("substring index %d out of range", List_size(formatting_info.substrings))
                                return NULL;
                        }
                }else if(arg->type==BUFFER){
                       for(int i=0; i<List_size(args); i++)
                       {
                               struct info_internal_format_substring *sstr = List_get(formatting_info.substrings, i);
                               if(!strcmp(sstr->name, info_internal_buffer_str(arg->buf))){
                                       index=i;
                                       goto fin;
                               }
                       }
                       INTERNAL_ERROR("substring \"%s\" could not be foud!", info_internal_buffer_str(arg->buf))
                        return NULL;
                }
        }
fin:
        struct info_internal_format_substring *sstr = List_get(formatting_info.substrings, index);
        List out = List_create(sizeof(struct info_internal_drawcall));

        size_t lenght = sstr->offset_end-sstr->offset_start;
        struct info_internal_drawcall *d = List_append(out, NULL);
        d->ansi=formatting_info.current->start;
        d->content_stream=TEXT;
        d->content=info_internal_buffer_create(lenght);
        char *str_start = info_internal_buffer_str(formatting_info.buffer);
        info_internal_buffer_append(d->content, str_start+sstr->offset_start, lenght);

        return out;
}

List info_last_prefix(List args)
{
        if(List_size(args)){
                INTERNAL_ERROR("takes no arguments!")
                return NULL;
        }

        List out = List_create(sizeof(struct info_internal_drawcall));

        struct info_internal_drawcall *d = List_append(out, NULL);
        d->ansi=formatting_info.current->start;
        d->content_stream=TEXT;
        d->content=info_internal_buffer_create(last_prefix_buff_length);
        info_internal_buffer_append(d->content, last_prefix_buff, last_prefix_buff_length);

        return out;
}

struct info_format_function functions[] = {
        {"timestamp", 't', timestamp},
        {"type", 'i', info_tag},
        {"sub_beg", '(', substring_start},
        {"sub_end", ')', substring_end},
        {"content", 'c', info_content},
        {"function", 'f', info_funcname},
        {"whitespaces", 'w', info_whitespaces},
        {"indent", 'd', info_indentation},
        {"substring", 's', info_substring_get},
        {"last_prefix", 'p', info_last_prefix},


};

size_t functions_count = sizeof(functions)/sizeof(*functions);
