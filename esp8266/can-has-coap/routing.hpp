#pragma once
#include <cstring>
#include <cstdlib>

#define CAPTURE_UID ""

char *strndup(const char *str, size_t maxlen);

template <typename Req>
using Handler = void (*)(Req &request, char **captures, size_t ncaptures);

template <typename Req>
class RouteNode {
	public:
		virtual ~RouteNode() {};
		virtual bool match(char **url, char ***captures, size_t *ncaptures) = 0;
		const char *uid;
		RouteNode **children = nullptr;
		size_t nchildren = 0;
		Handler<Req> handler = 0;
		void debugPrint(int level = 0) {
			debugf("%d %s\t\t%p", level, this->uid, this->handler);
			for (size_t i = 0; i < this->nchildren; i++) {
				RouteNode<Req> *child = this->children[i];
				child->debugPrint(level + 1);
			}
		}
};


template <typename Req>
class BlankRouteNode: public RouteNode<Req> {
	public:
		BlankRouteNode() {}
		virtual bool match(char **url, char ***captures, size_t *ncaptures) {
			return true;
		}
};

template <typename Req>
class MatchRouteNode: public RouteNode<Req> {
	private:
		size_t len;

	public:
		MatchRouteNode(const char *part) {
			this->uid = part;
			this->len = strlen(part);
		}

		virtual bool match(char **url, char ***captures, size_t *ncaptures) {
			if (strncmp(this->uid, *url, this->len) == 0 &&
					(*(*url + this->len) == '/' || *(*url + this->len) == '\0')) {
				*url += this->len + 1;
				return true;
			}
			return false;
		}
};

template <typename Req>
class CaptureRouteNode: public RouteNode<Req> {
	public:
		CaptureRouteNode() {
			this->uid = CAPTURE_UID;
		}

		virtual bool match(char **url, char ***captures, size_t *ncaptures) {
			size_t len;
			char* endchar = strchr(*url, '/');
			if (endchar != NULL) {
				len = endchar - *url;
			} else {
				len = strlen(*url);
			}
			size_t i = *ncaptures;
			*captures = (char**)realloc(*captures, (++*ncaptures) * sizeof(char*));
			(*captures)[i] = strndup(*url, len);
			*url += len + 1;
			return true;
		}
};


template <typename Req>
class RouteMaker {
	public:
		RouteNode<Req> *node;
		RouteMaker(RouteNode<Req> *node) {
			this->node = node;
		}

		inline void operator=(Handler<Req> handler) {
			this->node->handler = handler;
		}
};

template <typename Req>
inline RouteMaker<Req> operator/(RouteMaker<Req> parent, const char *child_uid) {
	for (size_t i = 0; i < parent.node->nchildren; i++) {
		RouteNode<Req> *child = parent.node->children[i];
		if (strcmp(child->uid, child_uid) == 0)
			return RouteMaker<Req>(child);
	}
	size_t i = parent.node->nchildren;
	parent.node->children = (RouteNode<Req>**)realloc(parent.node->children, (++parent.node->nchildren) * sizeof(RouteNode<Req>*));
	parent.node->children[i] = new MatchRouteNode<Req>(child_uid);
	return RouteMaker<Req>(parent.node->children[i]);
}

template <typename Req>
inline RouteMaker<Req> operator/(RouteMaker<Req> parent, CaptureRouteNode<Req> &node) {
	for (size_t i = 0; i < parent.node->nchildren; i++) {
		RouteNode<Req> *child = parent.node->children[i];
		if (strcmp(child->uid, CAPTURE_UID) == 0)
			return RouteMaker<Req>(child);
	}
	size_t i = parent.node->nchildren;
	parent.node->children = (RouteNode<Req>**)realloc(parent.node->children, (++parent.node->nchildren) * sizeof(RouteNode<Req>*));
	parent.node->children[i] = &node;
	return RouteMaker<Req>(parent.node->children[i]);
}

template <typename Req>
class CoapRouter {
	private:
		BlankRouteNode<Req> root;

	public:
		inline friend RouteMaker<Req> operator/(CoapRouter<Req> &parent, const char *child_uid) {
			return RouteMaker<Req>(&parent.root) / child_uid;
		}

		inline friend RouteMaker<Req> operator/(CoapRouter<Req> &parent, CaptureRouteNode<Req> &node) {
			return RouteMaker<Req>(&parent.root) / node;
		}

		void debugPrint() {
			this->root.debugPrint();
		}

		Handler<Req> not_found;

		void handle(Req &request, char* url) {
			debugf("handling %s", url);
			char **murl = &url;
			char **captures = nullptr;
			size_t ncaptures = 0;
			RouteNode<Req> *cur_node = &this->root;
loop:
			for (size_t i = 0; i < cur_node->nchildren; i++) {
				if (cur_node->children[i]->match(murl, &captures, &ncaptures)) {
					cur_node = cur_node->children[i];
					goto loop;
				}
			}
			if (cur_node->handler != nullptr) {
				debugf("Calling matched handler uid %s", cur_node->uid);
				cur_node->handler(request, captures, ncaptures);
			} else if (this->not_found != nullptr) {
				debugf("Calling not found handler");
				this->not_found(request, captures, ncaptures);
			} else {
				debugf("No not found handler");
			}
			for (size_t i = 0; i < ncaptures; i++)
				free(captures[i]);
			free(captures);
		}
};
