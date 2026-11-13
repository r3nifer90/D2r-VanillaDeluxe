typedef unsigned long long U64;typedef unsigned int U32;typedef unsigned short U16;typedef unsigned char U8;
typedef U64(*Native)(U64,U64,U64,U64);typedef U64(*GetData)(U64);typedef void(*Log)(const void*,const char*);
extern Native original;
static const void*ctx;
static GetData item_data;
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int contains(const U8*b,U32 n,const char*s){for(U32 i=0;i<n;i++){U32 j=0;while(i+j<n&&s[j]&&b[i+j]==(U8)s[j])j++;if(!s[j])return 1;}return 0;}
static U32 ismap(const U8*b,U32 n){
 if(!contains(b,n," Map [Tier ")&&!contains(b,n," Map (Tier "))return 0;
 return contains(b,n,"Sandswept Tomb")||contains(b,n,"Corrupted Durance")||contains(b,n,"Forsaken Catacombs")||contains(b,n,"Frozen Depths")||contains(b,n,"Worldstone Keep")||contains(b,n,"Sunscar Dunes")||contains(b,n,"Forsaken Highlands")||contains(b,n,"Fallen Travincal")||contains(b,n,"Ashen Steppes")||contains(b,n,"Infernal Rift");
}
static void info(const char*s){const U8*a=*(const U8*const*)((const U8*)ctx+8);Log f=*(Log*)(a+8);if(f)f(ctx,s);}
static U8* module(void){
 U8*peb;__asm__("movq %%gs:0x60,%0":"=r"(peb));U8*ldr=*(U8**)(peb+0x18);U8*head=ldr+0x20;
 for(U8*e=*(U8**)head;e&&e!=head;e=*(U8**)e){U8*entry=e-0x10;U16 len=*(U16*)(entry+0x58);U16*name=*(U16**)(entry+0x60);const char*w="d2rcore.dll";if(len!=22||!name)continue;U32 i=0;for(;i<11;i++){U16 c=name[i];if(c>='A'&&c<='Z')c+=32;if(c!=(U8)w[i])break;}if(i==11)return *(U8**)(entry+0x30);}
 return (void*)0;
}
static U8* exported(U8*m,const char*name){
 if(!m||*(U16*)m!=0x5a4d)return 0;U32 pe=*(U32*)(m+0x3c);if(pe>4096||*(U32*)(m+pe)!=0x4550)return 0;
 U32 size=*(U32*)(m+pe+24+56);U32 er=*(U32*)(m+pe+24+112);if(er>=size||size-er<40)return 0;
 U8*e=m+er;U32 nn=*(U32*)(e+24),fa=*(U32*)(e+28),na=*(U32*)(e+32),oa=*(U32*)(e+36);if(nn>8192||na>=size||oa>=size||fa>=size)return 0;
 for(U32 i=0;i<nn;i++){U32 nr=*(U32*)(m+na+4*i);if(nr>=size)continue;if(eq((char*)(m+nr),name)){U16 ord=*(U16*)(m+oa+2*i);U32 addr=*(U32*)(m+fa+ord*4);if(addr>=size||(addr>=er&&addr<er+*(U32*)(m+pe+24+116)))return 0;return m+addr;}}
 return 0;
}
static U8* called(U8*p,U32 at){if(p[at]!=0xff||p[at+1]!=0x15)return 0;int rel=*(int*)(p+at+2);U8**slot=(U8**)(p+at+6+rel);return *slot;}
U32 recolor(U8*b,U32 n,U32 quality){
 if((quality!=4&&quality!=6)||!ismap(b,n))return 0;U8 color=quality==4?'9':'4';
 for(U32 i=0;i+2<n;i++){if(b[i]==255&&b[i+1]=='c'&&(b[i+2]=='3'||b[i+2]=='9'))b[i+2]=color;if(i+3<n&&b[i]==195&&b[i+1]==191&&b[i+2]=='c'&&(b[i+3]=='3'||b[i+3]=='9'))b[i+3]=color;}
 if(n>=1000)return 1;

 U32 start=0;if(n>=3&&b[0]==255&&b[1]=='c')start=3;else if(n>=4&&b[0]==195&&b[1]==191&&b[2]=='c')start=4;
 if(start){b[start-1]=color;return 1;}
 for(U32 i=n+1;i;i--)b[i+3]=b[i-1];b[0]=195;b[1]=191;b[2]='c';b[3]=color;return 1;
}
U64 name_hook(U64 unit,U64 buf,U64 c,U64 d){
 U64 result=original(unit,buf,c,d);if(!unit||!buf||!item_data)return result;
 U8*b=(U8*)buf;U32 n=0;while(n<1000&&b[n])n++;if(!n||n>=1000||!ismap(b,n))return result;
 U32*data=(U32*)item_data(unit);if(data)recolor(b,n,data[0]);return result;
}
_Bool load_colors(const void*context){
 ctx=context;if(!ctx||*(U32*)ctx<32)return 0;const U8*api=*(const U8*const*)((const U8*)ctx+8);if(!api||*(U32*)api<0xb8)return 0;
 U8*core=module();U8*getname=exported(core,"GetItemName");

 if(!getname||getname[0]!=0x55||getname[1]!=0x56||getname[2]!=0x57||getname[3]!=0x53||getname[0x5e]!=0x48||getname[0x5f]!=0x89||getname[0x60]!=0xf9){info("Map colors: unsupported D2RCore GetItemName wrapper; no patch installed.");return 0;}
 U8*target=called(getname,0x21);item_data=(GetData)called(getname,0x61);
 U8*game=*(U8**)((const U8*)ctx+0x18);if(!game||*(U16*)game!=0x5a4d){info("Map colors: invalid game module.");return 0;}U32 pe=*(U32*)(game+0x3c),size=*(U32*)(game+pe+24+56);
 if(!target||!item_data||target<game||target+32>game+size){info("Map colors: native name binding lies outside the game image; no patch installed.");return 0;}



 const U8 guard[]={0x48,0x85,0xc9,0x0f,0x84,0x09,0x15,0,0};
 const U8 body[]={0x55,0x56,0x41,0x54,0x48,0x8d,0xac,0x24};
 for(U32 i=0;i<sizeof guard;i++)if(target[i]!=guard[i]){info("Map colors: unexpected native entry guard; no hook installed.");return 0;}
 for(U32 i=0;i<sizeof body;i++)if(target[9+i]!=body[i]){info("Map colors: unexpected native body prologue; no hook installed.");return 0;}
 target+=9;
 struct Hook{U32 size,flags;U64 rva;const void*expected;U32 expected_size,padding;void*target;void**original;U64 reserved[4];};
 struct Hook h={0};h.size=sizeof h;h.rva=target-game;h.expected=target;h.expected_size=16;h.target=name_hook;h.original=(void**)&original;
 typedef _Bool(*Install)(const void*,const void*);Install install=*(Install*)(api+0xb0);if(!install||!install(ctx,&h)||!original){info("Map colors: native item-name hook could not be installed.");return 0;}
 info("Map colors 0.5.0: name body hooked after entry guard; Gheed entry preserved; magic yellow, rare gold.");return 1;
}
