#include <string.h>
#include "story_vm.h"
#include "name_keyboard.h"
#include "../generated/story_search.h"
#include "../generated/story_program.h"

unsigned story_menu_count,story_menu_text[STORY_MENU_CAPACITY];
unsigned story_text,story_speaker,story_event_arg,story_event_op,story_generation,story_event_pc;
unsigned story_event_arg2,story_event_arg3;
bool story_menu_enabled[STORY_MENU_CAPACITY];
static unsigned actions[STORY_MENU_CAPACITY],stack_count,budget;
/* Imported SP19 uses packed fields through9000 (byte288). Legacy slots
 * retain their128-byte prefix; sparse extension records preserve later flags. */
static uint8_t fields[320];
static uint32_t rng;
static int root;
static unsigned command_root;
static unsigned command_module=0x3f;
static unsigned last_music=0xffff;
static unsigned display_under=0xffff;
static uint16_t history[3];
static uint16_t history_modules[3];
static unsigned history_count;
static void remember_root(void){
    if(root<0)return;
    if(history_count==3){history[0]=history[1];history[1]=history[2];history_modules[0]=history_modules[1];history_modules[1]=history_modules[2];history_count=2;}
    history[history_count]=story_nodes[root].pc;history_modules[history_count++]=story_nodes[root].module_id;
}
static bool valid,building;
static bool battle_pending;
static unsigned battle_damage,battle_stage;
static uint16_t practice_values[7];
static bool practice_values_valid;
static const unsigned practice_addresses[7]={0x7908,0x790a,0x790c,0x790e,0x7914,0x7916,0x7918};
static bool native_parameter_supported(unsigned module,unsigned address){
    if(address==0x7972)return module==0x3f || module==0x40;
    if(module==0x40)return address==0x7910;
    if(module==0x3f || module==0x4c)for(unsigned i=0;i<7;i++)if(address==practice_addresses[i])return true;
    return false;
}
static bool native_parameter(unsigned module,unsigned address,unsigned *value){
    if(!native_parameter_supported(module,address))return false;
    if(address==0x7972){*value=0;return true;}
    if(module==0x40){*value=battle_damage;return true;}
    if(practice_values_valid)for(unsigned i=0;i<7;i++)if(address==practice_addresses[i]){
        *value=practice_values[i];return true;
    }
    return false;
}
typedef struct {int id;unsigned key,value,write;} Frame;
static Frame stack[64];
/* Original9840 pins a menu depth. A bounded continuation checkpoint lets
 * the DS event VM resume that same menu without replaying its entry speech.
 * Source SP04 only; later external keyboard modules require their own audit. */
typedef struct {unsigned count,command;Frame frames[64];} MenuPin;
static MenuPin menu_pins[8];
static unsigned menu_pin_count;
static unsigned name_lookup_phase;
static bool name_lookup_matched,name_input_pending;
static char name_query[18];
static unsigned name_input_kind,name_input_phase;
static bool contact_dial_mode;
/* SUBCODE16760 initializes98B2 to18 words: nine module/root pairs.
 * An external return rebuilds its outer caller, not the next instruction. */
typedef struct {uint16_t module,pc;} ModuleReturn;
static ModuleReturn module_returns[9];
static unsigned module_return_count;
typedef struct {unsigned count,root;uint16_t callers[64];} MenuContext;
static MenuContext menu_context[STORY_MENU_CAPACITY];
static uint16_t registration_callers[64];
static unsigned registration_depth;
unsigned story_read_field(unsigned key){
    unsigned at=(key>>7)*8+((key>>4)&7),width=(key&15)+1,result=0;
    if(at+width>sizeof(fields)*8){valid=false;return 0;}
    for(unsigned i=0;i<width;i++)result=(result<<1)|((fields[(at+i)/8]>>(7-(at+i)%8))&1);
    return result;
}
static void write_field(unsigned key,unsigned value){
    unsigned at=(key>>7)*8+((key>>4)&7),width=(key&15)+1;
    if(at+width>sizeof(fields)*8){valid=false;return;}
    for(unsigned i=0;i<width;i++){
        unsigned byte=(at+i)/8,bit=7-(at+i)%8;
        fields[byte]=(fields[byte]&~(1u<<bit))|(((value>>(width-i-1))&1u)<<bit);
    }
}
static bool spend(int id,unsigned depth){
    if(!valid || !budget || depth>64 || id<0 || id>=STORY_NODE_COUNT){valid=false;return false;}
    budget--;return true;
}
static int by_module_pc(unsigned module,unsigned pc){for(unsigned i=0;i<STORY_NODE_COUNT;i++)if(story_nodes[i].module_id==module && story_nodes[i].pc==pc && story_nodes[i].op<0x100)return i;return -1;}
static int by_pc(unsigned pc){return by_module_pc(command_module,pc);}
static void push(int id){if(id<0 || id>=STORY_NODE_COUNT || stack_count==64){valid=false;return;}stack[stack_count++]=(Frame){.id=id};}
static void later_write(unsigned key,unsigned value){if(stack_count==64){valid=false;return;}stack[stack_count++]=(Frame){.key=key,.value=value,.write=1};}
static void finish_pending_writes(void){
    // Unwind like the original recursive interpreter: innermost writers first.
    for(unsigned i=stack_count;i;i--)if(stack[i-1].write==1)write_field(stack[i-1].key,stack[i-1].value);
}
static unsigned condition(int id,unsigned depth){
    if(!spend(id,depth))return 0;
    const StoryNode *n=&story_nodes[id];
    if(n->op==0x100)return story_read_field(n->arg);
    if(n->op==0x101)return n->arg;
    if(n->op==0x2f)return story_read_field(n->arg)?0:0xffff;
    if(n->op==0x2e)return condition(n->left,depth+1)?0:0xffff;
    if(n->op==0x15)return n->arg && n->arg<=history_count?history[history_count-n->arg]:0xffff;
    if(n->op==0x3b){
        // SUBCODE file9E9E ->10788 searches the active 0C caller list (98E0),
        // not menu labels or the three-entry room history (98D8).
        if(!n->arg)return command_root;
        for(unsigned i=0;i<stack_count;i++)if(stack[i].write==2 && stack[i].key==n->arg)return 1;
        return 0;
    }
    unsigned a=condition(n->left,depth+1),b=condition(n->right,depth+1);
    if(n->op>=6 && n->op<=9)return a&b;
    if(n->op==0x31 || n->op==0x32 || n->op==0x33)return a|b; // SUBCODE A28A uses OR.W.
    if(n->op==1 || n->op==2 || n->op==3)return (a+b)&0xffff;
    // Descriptor01302352 selects fileA352/SCS, not fileA30A/SHI.
    if(n->op==0x16)return b<a?0xffff:0;
    if(n->op==0x17)return b<=a?0xffff:0;
    if(n->op==0x10 || n->op==0x11 || n->op==0x12)return a==b?0xffff:0;
    valid=false;return 0;
}
static bool popup_supported(unsigned module,unsigned picture,unsigned position){
    if((picture==9 && (position==5 || position==6 || (module==0x3f && (position==13 || position==14)))) ||
       (picture==10 && position==16) || (picture==14 && position==15) || ((picture==11 || picture==12) && position==13))return true;
    unsigned pair=(picture<<8)|position;
    if(module==0x41)return pair==0x000d || pair==0x020d || pair==0x050d || pair==0x070d || pair==0x0900 || pair==0x0f0d;
    if(module==0x4b)return pair==0x0900 || pair==0x0c0d || pair==0x0b0d || pair==0 || pair==0x0f00 || pair==0x0700 || pair==0x0200 || pair==0x0500;
    return false;
}
static bool supported_action(int id,unsigned depth){
    if(depth>64 || id<0 || id>=STORY_NODE_COUNT)return false;
    const StoryNode *n=&story_nodes[id];
    if(n->op==0x04)return (n->arg>=1185 && n->arg<=1191) || n->arg==1193 || (n->arg>=77 && n->arg<=201) || n->arg==0 || n->arg==1 || n->arg==2 || n->arg==5 || n->arg==6 || n->arg==7 || n->arg==8 || n->arg==9 || n->arg==16 || n->arg==20 || n->arg==21 || n->arg==28 || n->arg==32 || n->arg==37 || n->arg==40;
    if(n->op==0x43 || n->op==0x44)return story_text_supported[n->arg];
    if(n->op==0x3d && ((n->arg>=0x2000 && n->arg!=0x2000 && n->arg!=0x2002 && n->arg!=0x2003 && n->arg!=0x2004)))return false;
    if(n->op==0x1d && story_nodes[n->left].op==0x42){
        const StoryNode *p=&story_nodes[story_nodes[n->left].left];
        return n->arg==0x000f && p->op==0x23 && p->arg==5 && native_parameter_supported(n->module_id,p->arg2);
    }
    if(n->op==0x42){
        const StoryNode *p=&story_nodes[n->left];
        if(n->module_id==0x4b && p->op==0x23 && p->arg==11)return p->arg2<=1;
        if(p->op==0x23)return p->arg==4 && ((p->arg2==1 && n->module_id==0x40 && (n->pc==0x6b8 || n->pc==0x867)) || (p->arg2==0 && ((n->module_id==0x3f && n->pc==0x1716) || (n->module_id==0x41 && n->pc==0x20d5))));
        if(p->op==0x22)return (p->arg==8 && !p->arg2) || (p->arg==7 && p->arg2<=1);
        if(p->op==0x24 && p->arg==6){
            const StoryNode *value=&story_nodes[p->left];
            if(n->module_id==0x4b)return
                (value->op==0x23 && value->arg==0x971e && value->arg2==255) ||
                (value->op==0x22 && ((value->arg==0x971a && value->arg2==1) || (value->arg==0x9718 && value->arg2==12)));
            return n->module_id==0x40 || (n->module_id==0x3e && value->op==0x22 && value->arg==0x971a && value->arg2==1);
        }
        if(p->op!=0x24 || p->arg!=8)return false;
        p=&story_nodes[p->left];if(p->op!=0x1f || p->arg!=1)return false;
        p=&story_nodes[p->left];
        return p->op==0x22 && popup_supported(n->module_id,p->arg,p->arg2);
    }
    if(n->op==0x0c || n->op==0x2b)return by_pc(n->arg)>=0;
    if(n->op==0x0b){const StoryNode *p=&story_nodes[n->left];int target=by_module_pc(p->arg,p->arg2);return p->op==0x21 && target>=0 && story_nodes[target].op==0x30;}
    if(n->op==0x36)return n->module_id==0x3d && supported_action(n->left,depth+1);
    if(n->op==0x37 || n->op==0x39)return n->module_id==0x3d;
    if(n->op==0x41)return n->module_id==0x41 && n->arg==3;
    if(n->op==0x3a)return module_return_count>0;
    if(n->op==0x0d)return (n->module_id==0x3f && n->arg==20) || (n->module_id==0x41 && n->arg==6);
    if(n->op==0x2c){const StoryNode *p=&story_nodes[n->left];return p->op==0x21 && by_module_pc(p->arg,p->arg2)>=0;}
    if(n->op==0x25 || n->op==0x26){
        bool a=supported_action(n->left,depth+1),b=supported_action(n->right,depth+1);
        return n->menu_shape?a||b:a&&b;
    }
    if(n->op==0x19)return supported_action(story_read_field(n->arg)?n->left:n->right,depth+1);
    if(n->op==0x18)return supported_action(condition(n->left,0)?n->right:n->arg2,depth+1);
    if(n->op==0x1a)return !condition(n->left,0) || supported_action(n->right,depth+1);
    if(n->op==0x1b)return !story_read_field(n->arg) || supported_action(n->left,depth+1);
    if(n->op==0x27 || n->op==0x30 || n->op==0x34 || n->op==0x38 || n->op==0x3e || n->op==0x3f)return supported_action(n->left,depth+1);
    return true;
}
static bool menu_action_enabled(int id,unsigned text){
    (void)text;
    return supported_action(id,0);
}
/* A source0C can contribute entries to the caller's menu. Classify the
 * complete expression before presenting it; a call boundary is not a UI
 * boundary. Preserve native3B context while evaluating conditional rows. */
static bool menu_tree(int id,unsigned depth){
    if(id<0 || id>=STORY_NODE_COUNT || depth>=64)return false;
    const StoryNode *n=&story_nodes[id];
    switch(n->op){
    case 0x38:case 0x2d:return true;
    case 0x25:case 0x26:return menu_tree(n->left,depth+1) && menu_tree(n->right,depth+1);
    case 0x27:case 0x30:return menu_tree(n->left,depth+1);
    case 0x1b:return !story_read_field(n->arg) || menu_tree(n->left,depth+1);
    case 0x1a:return !condition(n->left,0) || menu_tree(n->right,depth+1);
    case 0x19:return menu_tree(story_read_field(n->arg)?n->left:n->right,depth+1);
    case 0x18:return menu_tree(condition(n->left,0)?n->right:n->arg2,depth+1);
    case 0x0c:{
        if(stack_count==64)return false;
        unsigned saved=command_root;
        stack[stack_count++]=(Frame){.key=saved,.value=command_module,.write=2};
        command_root=n->arg;bool result=menu_tree(by_pc(n->arg),depth+1);
        command_root=saved;stack_count--;return result;
    }
    default:return false;
    }
}
static void register_menu(int id,unsigned depth){
    if(!spend(id,depth))return;
    const StoryNode *n=&story_nodes[id];
    switch(n->op){
    case 0x25:case 0x26:register_menu(n->left,depth+1);register_menu(n->right,depth+1);break;
    case 0x27:case 0x30:register_menu(n->left,depth+1);break;
    case 0x1b:if(story_read_field(n->arg))register_menu(n->left,depth+1);break;
    case 0x1a:if(condition(n->left,depth+1))register_menu(n->right,depth+1);break;
    case 0x19:register_menu(story_read_field(n->arg)?n->left:n->right,depth+1);break;
    case 0x18:register_menu(condition(n->left,0)?n->right:n->arg2,depth+1);break;
    case 0x0c:{
        if(stack_count==64 || registration_depth==64){valid=false;break;}
        unsigned saved=command_root;
        registration_callers[registration_depth++]=saved;
        stack[stack_count++]=(Frame){.key=saved,.value=command_module,.write=2};
        command_root=n->arg;register_menu(by_pc(n->arg),depth+1);
        command_root=saved;stack_count--;registration_depth--;break;
    }
    case 0x38:
        if(name_lookup_phase){
            const char *key=NULL;
            for(unsigned i=0;i<STORY_SEARCH_KEY_COUNT;i++)if(story_search_keys[i].text==n->arg){key=story_search_keys[i].key;break;}
            if(!key){valid=false;break;}
            bool match=name_lookup_phase==1?strchr(key,name_query[0])!=NULL:strcmp(key,name_query)==0;
            if(story_menu_count && (name_lookup_matched || !match))break;
            /* Source defaults to the first registration when none match.
             * Keep just the selected source entry; hidden name tables can
             * exceed the ordinary16 visible-menu/save slots. */
            story_menu_count=0;name_lookup_matched=match;
        }
        if(story_menu_count==STORY_MENU_CAPACITY || n->arg>=STORY_TEXT_COUNT){valid=false;break;}
        story_menu_text[story_menu_count]=n->arg;actions[story_menu_count]=n->left;
        // Reapply module boundaries both here and when importing a saved menu.
        story_menu_enabled[story_menu_count]=menu_action_enabled(n->left,n->arg);
        menu_context[story_menu_count].count=registration_depth;
        menu_context[story_menu_count].root=command_root;
        memcpy(menu_context[story_menu_count].callers,registration_callers,registration_depth*sizeof(uint16_t));
        story_menu_count++;break;
    case 0x2d:break;
    default:valid=false;break;
    }
}
static void add_alternative(int id,int *choices,unsigned *count){
    if(id<0 || id>=STORY_NODE_COUNT){valid=false;return;}
    const StoryNode *n=&story_nodes[id];unsigned weight=(n->op==0x34 || n->op==0x35)?n->arg:1;
    int action=n->op==0x34?n->left:id;
    if(!weight || weight>32-*count){valid=false;return;}
    while(weight--)choices[(*count)++]=action;
}
static void refresh_menu(int id,unsigned depth){
    if(!spend(id,depth))return;
    const StoryNode *n=&story_nodes[id];
    if(menu_tree(id,depth)){register_menu(id,depth+1);return;}
    if(n->op==0x25 || n->op==0x26){refresh_menu(n->left,depth+1);refresh_menu(n->right,depth+1);}
    else if(n->op==0x27 || n->op==0x30)refresh_menu(n->left,depth+1);
    else if(n->op==0x1b){if(story_read_field(n->arg))refresh_menu(n->left,depth+1);}
    else if(n->op==0x1a){if(condition(n->left,depth+1))refresh_menu(n->right,depth+1);}
    else if(n->op==0x19)refresh_menu(story_read_field(n->arg)?n->left:n->right,depth+1);
    else if(n->op==0x18)refresh_menu(condition(n->left,depth+1)?n->right:n->arg2,depth+1);
    else if(n->op==0x38)register_menu(id,depth+1);
}
static void alternatives(int id,int *choices,unsigned *count,unsigned depth){
    if(!spend(id,depth))return;
    const StoryNode *n=&story_nodes[id];
    if(n->op==0x25 || n->op==0x26){add_alternative(n->left,choices,count);alternatives(n->right,choices,count,depth+1);}
    else add_alternative(n->op==0x27?n->left:id,choices,count);
}
static int menu_ready(void){
    /* A module jump can leave the prior module's last dialogue selected.
     * A menu save needs a text reference belonging to its current module. */
    if(story_menu_count && story_text_modules[story_text]!=story_nodes[root].module_id)
        story_text=story_menu_text[0];
    if(name_lookup_phase){
        if(!story_menu_count){valid=false;return STORY_ERROR;}
        name_lookup_phase=name_lookup_phase==1?2:0;name_lookup_matched=false;
        return story_pick(0);
    }
    return STORY_MENU;
}
static int rebuild(void){
    if(story_nodes[root].module_id==0x3d && menu_pin_count){
        MenuPin *pin=menu_pins+menu_pin_count-1;
        stack_count=pin->count;memcpy(stack,pin->frames,stack_count*sizeof(Frame));
        command_root=pin->command;command_module=0x3d;story_menu_count=0;building=false;
        return story_advance();
    }
    stack_count=story_menu_count=0;command_root=story_nodes[root].pc;command_module=story_nodes[root].module_id;building=true;push(root);
    if(!valid)return STORY_ERROR;
    return story_advance();
}
int story_advance(void){
    if(battle_pending)return STORY_BATTLE;
    story_generation++;budget=2048;
    while(valid && stack_count){
        // A conditional submenu may register several fragments. Finish those
        // registrations, but retain its caller context until the choice runs.
        if(!building && story_menu_count && stack[stack_count-1].write==2)return menu_ready();
        Frame f=stack[--stack_count];
        if(f.write==1){write_field(f.key,f.value);continue;}
        if(f.write==2){command_root=f.key;command_module=f.value;continue;}
        if(!spend(f.id,0))break;
        const StoryNode *n=&story_nodes[f.id];
        if(n->op!=0x2d && (n->menu_shape || menu_tree(f.id,0))){
            register_menu(f.id,0);
            if(!valid)break;
            continue;
        }
        story_event_pc=n->pc;story_event_op=n->op;story_event_arg=n->arg;
        switch(n->op){
        case 0x25:case 0x26:push(n->right);push(n->left);break;
        case 0x27:case 0x30:case 0x34:push(n->left);break;
        case 0x2d:case 0x35:break;
        case 0x46:return STORY_AUDIO_WAIT;
        case 0x14:write_field(n->arg,0xffff);break;
        case 0x13:write_field(n->arg,0);break;
        case 0x0e:{unsigned v=story_read_field(n->arg),mask=(1u<<((n->arg&15)+1))-1;if(v<mask)write_field(n->arg,v+1);break;}
        case 0x1c:case 0x1d:
            if(story_nodes[n->left].op==0x42){
                const StoryNode *p=&story_nodes[story_nodes[n->left].left];
                unsigned value;
                if(n->arg!=0x000f || p->op!=0x23 || p->arg!=5 || !native_parameter(n->module_id,p->arg2,&value)){valid=false;break;}
                write_field(n->arg,value);break;
            }
            write_field(n->arg,condition(n->left,0));break;
        case 0x1b:if(story_read_field(n->arg))push(n->left);break;
        case 0x1a:if(condition(n->left,0))push(n->right);break;
        case 0x19:push(story_read_field(n->arg)?n->left:n->right);break;
        case 0x18:push(condition(n->left,0)?n->right:n->arg2);break;
        case 0x0c:
            if(n->module_id==0x40 && n->arg==0x185e && battle_damage>=6)return STORY_GAME_OVER;
            if(stack_count==64){valid=false;break;}
            stack[stack_count++]=(Frame){.key=command_root,.value=command_module,.write=2};
            command_root=n->arg;push(by_pc(n->arg));break;
        case 0x2b:
            menu_pin_count=0;
            finish_pending_writes();
            remember_root();
            stack_count=story_menu_count=0;root=by_pc(n->arg);building=true;
            if(root<0){if(n->arg==0x5ff || n->arg==0x6a0)return STORY_SCENE;valid=false;break;}
            command_root=n->arg;push(root);break;
        case 0x2c:{
            menu_pin_count=0;
            const StoryNode *p=&story_nodes[n->left];
            if(p->op!=0x21 || by_module_pc(p->arg,p->arg2)<0){valid=false;break;}
            finish_pending_writes();remember_root();root=by_module_pc(p->arg,p->arg2);command_root=p->arg2;command_module=p->arg;stack_count=story_menu_count=0;building=true;push(root);break;
        }
        case 0x0b:{
            const StoryNode *p=&story_nodes[n->left];int target=by_module_pc(p->arg,p->arg2);
            if(p->op!=0x21 || target<0 || story_nodes[target].op!=0x30 || module_return_count==9){valid=false;break;}
            menu_pin_count=0;
            unsigned caller=command_root;
            for(unsigned i=0;i<stack_count;i++)if(stack[i].write==2){caller=stack[i].key;break;}
            module_returns[module_return_count++]=(ModuleReturn){command_module,caller};
            /* Native guard16C90 suppresses the remaining expression after
             * setting9802, while recursive3F writes still unwind. */
            finish_pending_writes();root=target;command_root=p->arg2;command_module=p->arg;
            stack_count=story_menu_count=0;building=true;push(root);break;
        }
        case 0x3a:{
            if(!module_return_count){valid=false;break;}
            menu_pin_count=0;
            ModuleReturn caller=module_returns[--module_return_count];
            int target=by_module_pc(caller.module,caller.pc);
            if(target<0 || story_nodes[target].op!=0x30){valid=false;break;}
            finish_pending_writes();root=target;command_root=caller.pc;command_module=caller.module;
            stack_count=story_menu_count=0;building=true;push(root);break;
        }
        case 0x36:case 0x37:{
            if(n->module_id!=0x3d || menu_pin_count==8 || !stack_count){valid=false;break;}
            MenuPin *pin=menu_pins+menu_pin_count++;
            pin->count=stack_count;pin->command=command_root;
            memcpy(pin->frames,stack,stack_count*sizeof(Frame));
            if(n->op==0x36)push(n->left);
            break;
        }
        case 0x39:
            if(n->module_id!=0x3d){valid=false;break;}
            if(menu_pin_count)menu_pin_count--;
            break;
        case 0x3e:case 0x3f:{
            int choices[32];unsigned count=0,choice;alternatives(n->left,choices,&count,0);
            if(!valid || !count){valid=false;break;}
            if(n->op==0x3f)choice=story_read_field(n->arg);
            else{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;choice=rng%count;}
            if(choice>=count){valid=false;break;}
            if(n->op==0x3e || choice+1<count)later_write(n->arg,n->op==0x3f?choice+1:choice);
            push(choices[choice]);break;
        }
        case 0x43:case 0x44:
            if(n->arg>=STORY_TEXT_COUNT || n->speaker>=49){valid=false;break;}
            story_text=n->arg;story_speaker=n->speaker;return STORY_DIALOG;
        case 0x04:return STORY_VOICE;
        case 0x4a:return STORY_WAIT;
        case 0x00:
            if(n->left<0 || story_nodes[n->left].op!=0x23){valid=false;break;}
            story_event_arg=story_nodes[n->left].arg;
            story_event_arg2=story_nodes[n->left].arg2;return STORY_GRAPHICS;
        case 0x42:{
            const StoryNode *request=&story_nodes[n->left];
            if(n->module_id==0x4b && request->op==0x23 && request->arg==11 && request->arg2<=1){
                story_event_arg=11;story_event_arg2=request->arg2;story_event_arg3=0;return STORY_GRAPHICS;
            }
            if((n->module_id==0x3e || n->module_id==0x4b) && request->op==0x24 && request->arg==6){
                const StoryNode *p=&story_nodes[request->left];
                if(n->module_id==0x4b && p->op==0x23 && p->arg==0x971e && p->arg2==255){contact_dial_mode=true;break;}
                if(p->op==0x22 && p->arg==0x971a && p->arg2==1){
                    if(n->module_id==0x4b && !contact_dial_mode){valid=false;break;}
                    name_input_kind=n->module_id==0x4b?NAME_INPUT_PHONE:NAME_INPUT_PERSON;name_input_phase=1;
                }else if(n->module_id==0x4b && p->op==0x22 && p->arg==0x9718 && p->arg2==12){
                    name_input_kind=NAME_INPUT_PASSWORD;name_input_phase=2;
                }else{valid=false;break;}
                name_input_pending=true;name_lookup_phase=0;name_lookup_matched=false;
                return STORY_KEYBOARD;
            }
            if(request->op==0x23 && request->arg==4 && request->arg2==1 && n->module_id==0x40 && (n->pc==0x6b8 || n->pc==0x867)){battle_pending=true;battle_stage=1;story_event_arg=1;return STORY_BATTLE;}
            if(request->op==0x23 && request->arg==4 && request->arg2==0 && ((n->module_id==0x3f && n->pc==0x1716) || (n->module_id==0x41 && n->pc==0x20d5))){
                battle_pending=true;battle_stage=0;practice_values_valid=false;memset(practice_values,0,sizeof(practice_values));
                story_event_arg=0;return STORY_BATTLE;
            }
            if(request->op==0x24 && request->arg==6 && n->module_id==0x40){
                const StoryNode *p=&story_nodes[request->left];
                if(p->op!=0x22 || p->arg!=65535 || p->arg2>1){valid=false;break;}
                story_event_arg=6;story_event_arg2=p->arg2;story_event_arg3=0;return STORY_GRAPHICS;
            }
            if(request->op==0x22 && request->arg==7 && request->arg2<=1){
                story_event_arg=7;story_event_arg2=request->arg2;story_event_arg3=0;return STORY_GRAPHICS;
            }
            if(request->op==0x22 && request->arg==8 && request->arg2==0){
                story_event_arg=8;story_event_arg2=0;story_event_arg3=0;return STORY_GRAPHICS;
            }
            if(request->op!=0x24 || request->arg!=8){valid=false;break;}
            const StoryNode *on=&story_nodes[request->left];
            if(on->op!=0x1f || on->arg!=1){valid=false;break;}
            const StoryNode *params=&story_nodes[on->left];
            if(params->op!=0x22 || !popup_supported(n->module_id,params->arg,params->arg2)){valid=false;break;}
            story_event_arg=8;story_event_arg2=1;story_event_arg3=(params->arg<<8)|params->arg2;return STORY_GRAPHICS;
        }
        case 0x0d:
            if(!((n->module_id==0x3f && n->arg==20) || (n->module_id==0x41 && n->arg==6))){valid=false;break;}
            return STORY_GRAPHICS; // Resource20 is the observed flight sequence.
        case 0x48:return STORY_MEDIA_WAIT;
        case 0x41:
            if(n->module_id!=0x41 || n->arg!=3){valid=false;break;}
            return STORY_GRAPHICS;
        case 0x47:case 0x49:return STORY_GRAPHICS; // Primary packet19; UI must handle explicitly.
        case 0x0a:if(n->arg)last_music=n->arg;return STORY_GRAPHICS;
        case 0x05:case 0x0f:case 0x3d:case 0x45:return STORY_GRAPHICS;
        default:valid=false;break;
        }
    }
    if(!valid)return STORY_ERROR;
    if(!building && story_menu_count)return menu_ready();
    if(building){
        story_menu_count=0;refresh_menu(root,0);
        /* Original SP08/206E is a three-pass3F root: run practice,
         * call SP19, then clear4341 and return to the range menu. */
        if(valid && !story_menu_count && story_nodes[root].module_id==0x41 && story_nodes[root].pc==0x206e)return rebuild();
        if(!valid || !story_menu_count){valid=false;return STORY_ERROR;}
        return menu_ready();
    }
    return rebuild();
}
int story_start(void){root=STORY_START_NODE;command_module=0x3f;command_root=story_nodes[root].pc;return rebuild();}
int story_start_after_entry(void){
    /* Rebuild the real expression stack, consuming the scene, wait and
     * sound requests already executed by the native SP06 prefix. */
    int event=story_start();
    if(event!=STORY_GRAPHICS || story_event_pc!=0x4f || story_event_op!=0x3d || story_event_arg!=0x802)return STORY_ERROR;
    event=story_advance();
    if(event!=STORY_WAIT || story_event_pc!=0x53 || story_event_arg!=60)return STORY_ERROR;
    event=story_advance();
    if(event!=STORY_GRAPHICS || story_event_pc!=0x57 || story_event_op!=0x0a || story_event_arg!=0x4f)return STORY_ERROR;
    event=story_advance();
    return event==STORY_DIALOG && story_event_pc==0x5b?event:STORY_ERROR;
}
int story_after_native_flight(void){
    /* Consume exactly the already executed original departure expression.
     * Keep the real pending stack and let2C21 perform its module handoff. */
    if(story_module()!=0x3f || story_event_pc!=0x1f46 || story_event_op!=0x0a || story_event_arg!=255)return STORY_ERROR;
    static const unsigned pcs[]={0x1f4a,0x1f4e,0x1f52,0x1f54,0x1f5b,0x1f62,0x1f66,0x1f6a,0x1f6e,0x1f72,0x1f74,0x1f7b,0x1f82,0x1f86};
    static const unsigned ops[]={0x4a,0x04,0x46,0x00,0x00,0x0d,0x45,0x45,0x3d,0x48,0x00,0x00,0x45,0x4a};
    for(unsigned i=0;i<sizeof(pcs)/sizeof(*pcs);i++){
        if(story_advance()==STORY_ERROR || story_event_pc!=pcs[i] || story_event_op!=ops[i])return STORY_ERROR;
    }
    /* The live S0 player also executes SP07's arrival animation, score45
     * and actor17 completion. Keep its first authored dialogue pending. */
    static const unsigned arrival_pcs[]={0x06,0x0a,0x11,0x15};
    static const unsigned arrival_ops[]={0x3d,0x00,0x0a,0x45};
    for(unsigned i=0;i<4;i++){
        if(story_advance()!=STORY_GRAPHICS || story_module()!=0x40 ||
           story_event_pc!=arrival_pcs[i] || story_event_op!=arrival_ops[i])return STORY_ERROR;
    }
    return story_advance();
}
int story_after_native_forecourt(void){
    if(story_module()!=0x40 || story_event_pc!=0x74 || story_event_op!=0x3d || story_event_arg!=0x1905)return STORY_ERROR;
    static const unsigned pcs[]={0x78,0x7c,0x83,0x87,0x8b,0x8f,0x91};
    static const unsigned ops[]={0x4a,0x00,0x0a,0x4a,0x04,0x46,0x00};
    for(unsigned i=0;i<sizeof(pcs)/sizeof(*pcs);i++){
        if(story_advance()==STORY_ERROR || story_module()!=0x40 || story_event_pc!=pcs[i] || story_event_op!=ops[i])return STORY_ERROR;
    }
    return story_advance();
}
int story_start_at(unsigned pc,const uint8_t in[132]){
    menu_pin_count=module_return_count=0;
    memset(fields+128,0,sizeof(fields)-128);
    command_module=0x3f;
    if(in){memcpy(fields,in,128);rng=0;for(unsigned i=0;i<4;i++)rng|=(uint32_t)in[128+i]<<(i*8);}
    // Legacy office checkpoints already display the first office sequence.
    if(in && pc==0x797 && !story_read_field(0x2360))write_field(0x2360,1);
    root=by_pc(pc);command_root=pc;valid=root>=0;story_menu_count=stack_count=0;return rebuild();
}
unsigned story_text_offset(void){return story_text_offsets[story_text];}
int story_pick(unsigned option){
    if(!valid || option>=story_menu_count || !story_menu_enabled[option])return STORY_ERROR;
    int action=actions[option];MenuContext *context=&menu_context[option];
    if(stack_count>=64 || context->count>63-stack_count)return STORY_ERROR;
    for(unsigned i=0;i<context->count;i++)stack[stack_count++]=(Frame){.key=context->callers[i],.value=command_module,.write=2};
    if(context->count)command_root=context->root;
    story_menu_count=0;building=false;push(action);return story_advance();
}
int story_submit_name(const char *name){
    if(!name_input_pending || (story_module()!=0x3e && story_module()!=0x4b) || !name)return STORY_ERROR;
    unsigned length=0;bool nonspace=false;
    while(name[length]){
        unsigned ch=(unsigned char)name[length];
        if(length==name_keyboard_limit(name_input_kind) || !name_keyboard_accepts(name_input_kind,ch))return STORY_ERROR;
        nonspace|=ch!=' ';length++;
    }
    if(name_input_kind==NAME_INPUT_PHONE && (!length || length!=name_keyboard_dial_length((unsigned char)name[0])))return STORY_ERROR;
    memcpy(name_query,name,length+1);
    if(!nonspace){name_query[0]=' ';name_query[1]=0;}
    name_input_pending=false;name_lookup_phase=name_input_phase;name_lookup_matched=false;contact_dial_mode=false;
    return story_advance();
}
unsigned story_keyboard_kind(void){return name_input_kind;}
int story_cancel(void){name_lookup_phase=0;name_input_pending=false;contact_dial_mode=false;return rebuild();}
void story_reset(void){contact_dial_mode=false;name_input_kind=NAME_INPUT_PERSON;name_input_phase=1;name_lookup_phase=0;name_input_pending=false;menu_pin_count=module_return_count=0;last_music=display_under=0xffff;battle_pending=false;battle_damage=battle_stage=0;practice_values_valid=false;memset(practice_values,0,sizeof(practice_values));registration_depth=0;memset(menu_context,0,sizeof(menu_context));memset(fields,0,sizeof(fields));memset(history,0,sizeof(history));memset(history_modules,0,sizeof(history_modules));history_count=0;rng=0x534e4453;stack_count=story_menu_count=story_generation=0;valid=true;building=true;root=STORY_START_NODE;command_root=story_nodes[root].pc;command_module=0x3f;}
unsigned story_module(void){return story_nodes[root].module_id;}
unsigned story_music_request(void){return last_music;}
bool story_set_music_request(unsigned command){
    if(!command || command>255)return false;
    last_music=command;return true;
}
unsigned story_display_under(void){return display_under;}
void story_set_display_under(unsigned scene){display_under=scene&0xffff;}
static void put16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static unsigned get16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static int text_by_offset(unsigned module,unsigned offset){for(unsigned i=0;i<STORY_TEXT_COUNT;i++)if(story_text_modules[i]==module && story_text_offsets[i]==offset)return i;return -1;}
void story_export_fields(uint8_t out[132]){memcpy(out,fields,128);for(unsigned i=0;i<4;i++)out[128+i]=rng>>(i*8);}
void story_export(uint8_t out[STORY_STATE_BYTES]){
    memset(out,0,STORY_STATE_BYTES);out[0]=9;out[1]=valid && !name_input_pending && !name_lookup_phase;out[2]=building;
    put16(out+22,story_nodes[root].module_id);
    for(unsigned i=0;i<4;i++)out[4+i]=rng>>(i*8);
    put16(out+8,story_nodes[root].pc);put16(out+10,stack_count);put16(out+12,story_menu_count);
    put16(out+14,story_text_offsets[story_text]);put16(out+16,story_speaker);put16(out+18,story_event_arg);put16(out+20,story_event_op);
    memcpy(out+24,fields,128);
    for(unsigned i=0;i<story_menu_count;i++){put16(out+152+i*2,story_text_offsets[story_menu_text[i]]);put16(out+184+i*2,story_nodes[actions[i]].pc);}
    for(unsigned i=0;i<stack_count;i++){
        uint8_t *p=out+216+i*8;put16(p,stack[i].write?0:story_nodes[stack[i].id].pc);put16(p+2,stack[i].write?stack[i].key:story_nodes[stack[i].id].module_id);put16(p+4,stack[i].value);put16(p+6,stack[i].write);
    }
    /* Context records share the existing64-frame save area. Ordinary old
     * frames retain their byte layout; no game-slot size or filename change. */
    unsigned saved_frames=stack_count;
    for(unsigned i=0;i<story_menu_count;i++)for(unsigned j=0;j<menu_context[i].count;j++){
        if(saved_frames==64){out[1]=0;return;}
        uint8_t *p=out+216+saved_frames++*8;
        put16(p,story_nodes[actions[i]].pc);put16(p+2,menu_context[i].callers[j]);
        put16(p+4,j+1<menu_context[i].count?menu_context[i].callers[j+1]:menu_context[i].root);put16(p+6,3);
    }
    for(unsigned i=0;i<module_return_count;i++){
        if(saved_frames==64){out[1]=0;return;}
        uint8_t *p=out+216+saved_frames++*8;
        put16(p,module_returns[i].pc);put16(p+2,module_returns[i].module);put16(p+6,4);
    }
    for(unsigned i=128;i<sizeof(fields);i+=4){
        if(!(fields[i]|fields[i+1]|fields[i+2]|fields[i+3]))continue;
        if(saved_frames==64){out[1]=0;return;}
        uint8_t *p=out+216+saved_frames++*8;
        put16(p,i);memcpy(p+2,fields+i,4);put16(p+6,5);
    }
    for(unsigned i=0;i<menu_pin_count;i++){
        MenuPin *pin=menu_pins+i;
        if(saved_frames+1+pin->count>64){out[1]=0;return;}
        uint8_t *p=out+216+saved_frames++*8;
        put16(p,pin->count);put16(p+2,pin->command);put16(p+4,0x3d);put16(p+6,6);
        for(unsigned j=0;j<pin->count;j++){
            Frame *f=pin->frames+j;p=out+216+saved_frames++*8;
            put16(p,f->write?0:story_nodes[f->id].pc);
            put16(p+2,f->write?f->key:story_nodes[f->id].module_id);
            put16(p+4,f->value);put16(p+6,7+f->write);
        }
    }
    put16(out+10,saved_frames);
    out[728]=history_count;for(unsigned i=0;i<history_count;i++)put16(out+730+i*2,history[i]);
    put16(out+736,command_root);
    for(unsigned i=0;i<history_count;i++)put16(out+738+i*2,history_modules[i]);
    put16(out+744,command_module);
    put16(out+746,last_music);
    put16(out+748,display_under);
    if(practice_values_valid){for(unsigned i=0;i<7;i++)put16(out+750+i*2,practice_values[i]);out[764]=1;}
}
bool story_blob_valid(const uint8_t in[STORY_STATE_BYTES]){
    unsigned module=in[0]>=3?get16(in+22):0x3f;
    int root_id=by_module_pc(module,get16(in+8));
    if(in[0]<1 || in[0]>9 || in[1]!=1 || in[2]>1 || in[3] || (in[0]<3 && (in[22] || in[23])) || root_id<0 || get16(in+10)>64 || get16(in+12)>STORY_MENU_CAPACITY || text_by_offset(module,get16(in+14))<0 || get16(in+16)>=49)return false;
    if(story_nodes[root_id].op!=0x30)return false;
    unsigned count=get16(in+12),pending=get16(in+10),returns=0;
    uint64_t extension_blocks=0;
    unsigned pins=0,pin_remaining=0;
    for(unsigned i=0;i<count;i++)if(text_by_offset(module,get16(in+152+i*2))<0 || by_module_pc(module,get16(in+184+i*2))<0)return false;
    for(unsigned i=0;i<pending;i++){
        const uint8_t *p=in+216+i*8;unsigned id=get16(p),key=get16(p+2),write=get16(p+6);
        if(write>(in[0]==1?1:in[0]<4?2:in[0]<8?3:in[0]<9?5:9))return false;
        if(pin_remaining){
            if(write<7 || write>9)return false;
            if(write==7 && by_module_pc(key,id)<0)return false;
            if(write==8 && (id || (key>>7)*8+((key>>4)&7)+(key&15)+1>sizeof(fields)*8))return false;
            if(write==9 && (id || by_module_pc(get16(p+4),key)<0))return false;
            pin_remaining--;continue;
        }
        if(write>=7)return false;
        if(write==6){
            if(module!=0x3d || get16(p+4)!=0x3d || ++pins>8 || !id || id>64 || i+id>=pending || by_module_pc(0x3d,key)<0)return false;
            pin_remaining=id;continue;
        }
        if(write==0 && by_module_pc(in[0]>=3?key:0x3f,id)<0)return false;
        if(write==1 && (key>>7)*8+((key>>4)&7)+(key&15)+1>sizeof(fields)*8)return false;
        if(write==2 && by_module_pc(in[0]>=3?get16(p+4):0x3f,key)<0)return false;
        if(write==3){
            bool found=false;for(unsigned j=0;j<count;j++)if(get16(in+184+j*2)==id)found=true;
            if(!found || by_module_pc(module,key)<0 || by_module_pc(module,get16(p+4))<0)return false;
        }
        if(write==4){
            int target=by_module_pc(key,id);
            if(++returns>9 || get16(p+4) || target<0 || story_nodes[target].op!=0x30)return false;
        }
        if(write==5){
            if(id<128 || id+4>sizeof(fields) || (id&3) || !(p[2]|p[3]|p[4]|p[5]))return false;
            uint64_t bit=UINT64_C(1)<<((id-128)/4);
            if(extension_blocks&bit)return false;
            extension_blocks|=bit;
        }
    }
    if(pin_remaining)return false;
    if(in[0]==1){for(unsigned i=728;i<STORY_STATE_BYTES;i++)if(in[i])return false;return true;}
    if(in[728]>3 || in[729])return false;
    for(unsigned i=0;i<in[728];i++)if(by_module_pc(in[0]>=3?get16(in+738+i*2):0x3f,get16(in+730+i*2))<0)return false;
    for(unsigned i=730+in[728]*2;i<736;i++)if(in[i])return false;
    if(by_module_pc(in[0]>=3?get16(in+744):0x3f,get16(in+736))<0)return false;
    if(in[0]==2){for(unsigned i=738;i<STORY_STATE_BYTES;i++)if(in[i])return false;}
    else{
        for(unsigned i=738+in[728]*2;i<744;i++)if(in[i])return false;
        if(in[0]>=5 && get16(in+746)>255 && get16(in+746)!=0xffff)return false;
        if(in[0]>=6){
            unsigned under=get16(in+748);
            if(under>3 && under!=0xffff){
                bool found=false;
                for(unsigned j=0;j<STORY_NODE_COUNT;j++)if(story_nodes[j].op==0x3d && (0x4000u|story_nodes[j].arg)==under)found=true;
                if(!found)return false;
            }
        }
        if(in[0]>=7){
            if(in[764]>1 || get16(in+752)>25 || get16(in+760)>1 || get16(in+762)>1)return false;
            if(!in[764])for(unsigned i=750;i<764;i++)if(in[i])return false;
        }
        for(unsigned i=in[0]>=7?765:in[0]>=6?750:in[0]>=5?748:746;i<STORY_STATE_BYTES;i++)if(in[i])return false;
    }
    return true;
}
bool story_import(const uint8_t in[STORY_STATE_BYTES]){
    if(!story_blob_valid(in))return false;
    contact_dial_mode=false;name_input_kind=NAME_INPUT_PERSON;name_input_phase=1;name_lookup_phase=0;name_input_pending=false;
    valid=true;building=in[2];rng=0;for(unsigned i=0;i<4;i++)rng|=(uint32_t)in[4+i]<<(i*8);
    last_music=in[0]>=5?get16(in+746):0xffff;
    display_under=in[0]>=6?get16(in+748):0xffff;
    unsigned module=in[0]>=3?get16(in+22):0x3f;
    battle_pending=false;root=by_module_pc(module,get16(in+8));stack_count=get16(in+10);story_menu_count=get16(in+12);story_text=text_by_offset(module,get16(in+14));story_speaker=get16(in+16);story_event_arg=get16(in+18);story_event_op=get16(in+20);
    history_count=in[728];memset(history,0,sizeof(history));memset(history_modules,0,sizeof(history_modules));for(unsigned i=0;i<history_count;i++){history[i]=get16(in+730+i*2);history_modules[i]=in[0]>=3?get16(in+738+i*2):0x3f;}
    memset(fields,0,sizeof(fields));memcpy(fields,in+24,128);
    battle_damage=story_read_field(0x000f);battle_stage=1;
    practice_values_valid=in[0]>=7 && in[764];
    for(unsigned i=0;i<7;i++)practice_values[i]=practice_values_valid?get16(in+750+i*2):0;
    command_root=in[0]>=2?get16(in+736):story_nodes[root].pc;command_module=in[0]>=3?get16(in+744):0x3f;
    // Released v1 blobs lost the first-hall 3F write on a jump. Repair only
    // established office/ID progress; fresh reception and partial introductions
    // must still take the original first-arrival branch.
    if(in[0]==1 && story_read_field(0x1b40) && story_read_field(0x1be0) && story_read_field(0x1bf0) && story_read_field(0x1c80) && !story_read_field(0x2360))write_field(0x2360,1);
    for(unsigned i=0;i<story_menu_count;i++){story_menu_text[i]=text_by_offset(module,get16(in+152+i*2));actions[i]=by_module_pc(module,get16(in+184+i*2));}
    unsigned saved_frames=stack_count;stack_count=0;module_return_count=menu_pin_count=0;memset(menu_context,0,sizeof(menu_context));
    for(unsigned i=0;i<saved_frames;i++){
        const uint8_t *p=in+216+i*8;unsigned kind=get16(p+6),key=get16(p+2);
        if(kind==6){
            MenuPin *pin=menu_pins+menu_pin_count++;
            pin->count=get16(p);pin->command=key;
            for(unsigned j=0;j<pin->count;j++){
                p=in+216+(++i)*8;unsigned type=get16(p+6)-7,k=get16(p+2);
                pin->frames[j]=(Frame){.id=type?0:by_module_pc(k,get16(p)),.key=k,.value=get16(p+4),.write=type};
            }
        }else if(kind==5){
            memcpy(fields+get16(p),p+2,4);
        }else if(kind==4){
            module_returns[module_return_count++]=(ModuleReturn){key,get16(p)};
        }else if(kind==3){
            for(unsigned j=0;j<story_menu_count;j++)if(story_nodes[actions[j]].pc==get16(p)){
                MenuContext *context=&menu_context[j];context->callers[context->count++]=key;context->root=get16(p+4);break;
            }
        }else stack[stack_count++]=(Frame){.id=kind?0:by_module_pc(in[0]>=3?key:0x3f,get16(p)),.key=key,.value=kind==2 && in[0]<3?0x3f:get16(p+4),.write=kind};
    }
    /* Action availability may itself test3B. Reconstruct the saved callers
     * before evaluating it; never use a previous session's stack. */
    budget=2048;registration_depth=0;
    for(unsigned i=0;i<story_menu_count;i++){
        unsigned saved_count=stack_count,saved_root=command_root;
        MenuContext *context=&menu_context[i];
        if(context->count>64-stack_count)return false;
        for(unsigned j=0;j<context->count;j++)stack[stack_count++]=(Frame){.key=context->callers[j],.value=command_module,.write=2};
        if(context->count)command_root=context->root;
        story_menu_enabled[i]=menu_action_enabled(actions[i],story_menu_text[i]);
        stack_count=saved_count;command_root=saved_root;
    }
    story_generation++;return true;
}
int story_blob_text(const uint8_t in[STORY_STATE_BYTES]){return text_by_offset(in[0]>=3?get16(in+22):0x3f,get16(in+14));}
int story_legacy_dialog(unsigned line){
    story_reset();int event=story_start();
    while(event==STORY_GRAPHICS || event==STORY_WAIT)event=story_advance();
    if(!line)return event;
    write_field(0x1b10,1);
    while(event!=STORY_MENU && event>0)event=story_advance();
    return event==STORY_MENU?story_pick(0):STORY_ERROR;
}

bool story_battle_result(unsigned damage){
    if(!battle_pending || battle_stage!=1 || damage>6)return false;
    battle_pending=false;battle_damage=damage;return true;
}

uint8_t *story_native_fields(void){return fields;}
int story_after_native_return(void){
    /* Consume only the precise source events already played by the native
     * timeline. Preserve the genuine VM call stack, history and packed flags. */
    static const uint16_t events[][3]={
{0x40,0x4ad,0x4},
{0x40,0x4b1,0x46},
{0x40,0x4b3,0x4},
{0x40,0x4b7,0x46},
{0x40,0x4b9,0x4},
{0x40,0x4bd,0x46},
{0x40,0x4bf,0x4},
{0x40,0x4c3,0x46},
{0x40,0x4c5,0x4},
{0x40,0x4c9,0x46},
{0x40,0x4cb,0x4},
{0x40,0x4cf,0x46},
{0x40,0x4d1,0x4},
{0x40,0x4d5,0x46},
{0x40,0x4d7,0xa},
{0x40,0x4db,0x4a},
{0x40,0xde0,0x0},
{0x40,0xde7,0x42},
{0x40,0xdf1,0x4},
{0x40,0xdf5,0x46},
{0x40,0xdf7,0x3d},
{0x40,0xdfb,0x4a},
{0x40,0xdff,0x4},
{0x40,0xe03,0xa},
{0x40,0xe07,0x46},
{0x40,0xe09,0x4},
{0x40,0xe0d,0x4a},
{0x40,0xe11,0x46},
{0x40,0xe13,0x4},
{0x40,0xe17,0x46},
{0x40,0xe19,0x4},
{0x40,0xe1d,0x46},
{0x40,0xe1f,0xa},
{0x40,0xe23,0x4a},
{0x40,0xe27,0x42},
{0x40,0xe2e,0x0},
{0x40,0xe35,0x4a},
{0x41,0x12,0x4a},
{0x41,0x16,0x3d},
{0x41,0x1a,0xd},
{0x41,0x1e,0x4a},
{0x41,0x22,0x0},
{0x41,0x29,0x45},
{0x41,0x2d,0x3d},
{0x41,0x31,0x45},
{0x41,0x35,0x3d},
{0x41,0x39,0x0},
{0x41,0x40,0x45},
{0x41,0x44,0x0},
{0x41,0x4b,0x45},
{0x41,0x53,0x3d},
{0x41,0x57,0x4a},
{0x41,0x5b,0x0},
{0x41,0x62,0x48},
{0x41,0x68,0xa}
    };
    for(unsigned i=0;i<sizeof(events)/sizeof(*events);i++){
        if(i && story_advance()==STORY_ERROR)return STORY_ERROR;
        if(story_module()!=events[i][0] || story_event_pc!=events[i][1] ||
           story_event_op!=events[i][2])return STORY_ERROR;
    }
    return story_advance();
}

bool story_practice_result(const uint16_t values[7]){
    if(!battle_pending || battle_stage!=0 || !values || values[1]>25 || values[5]>1 || values[6]>1)return false;
    memcpy(practice_values,values,sizeof(practice_values));practice_values_valid=true;battle_pending=false;return true;
}
